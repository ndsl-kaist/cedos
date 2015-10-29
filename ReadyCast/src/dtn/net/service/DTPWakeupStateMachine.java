package dtn.net.service;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Message;
import android.os.PowerManager;
import android.util.Log;
import dtn.util.statemachine.State;
import dtn.util.statemachine.StateMachine;

public class DTPWakeupStateMachine extends StateMachine {
	public static final String ACTION_DTP_ALARM = "dtn.net.service.Alarm";
	private static final int SCAN_TIMEOUT_MS = 1000;
	private static final int MIN_SCAN_INTERVAL_MS = 3*60*1000; /* 1 mins */
	
	static final int MSG_ALARM = 0;
	static final int MSG_SCAN_RESULTS_AVAILABLE = 1;
	static final int MSG_TIMEOUT = 2;

	private State mDefaultState = new DefaultState();
	private State mNormalState = new NormalState();
	private State mScanningState = new ScanningState();
	private State mProcessingState = new ProcessingState();
	
	private Context ctx;
	private BroadcastReceiver alarmReceiver;
	private BroadcastReceiver scanReceiver;
	private BroadcastReceiver connectivityReceiver;

	AlarmManager am;
	PowerManager pm;
	WifiManager wm;
	PowerManager.WakeLock wl = null;
	WifiInfo connectedAPInfo=null;
	
	long comming_alarm = -1;
	long last_scan = -1;
	
	public DTPWakeupStateMachine (Context context) {
		super("DTPWakeupStateMachine");
		ctx = context;
		
		Log.i("ReadycastWakeup", "DTPWakeupStateMachine: constructing..");
		
		am = (AlarmManager)context.getSystemService(Context.ALARM_SERVICE);
		wm = (WifiManager)ctx.getSystemService(Context.WIFI_SERVICE);
		pm = (PowerManager)ctx.getSystemService(Context.POWER_SERVICE);
		wl = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "");
		
		addState(mDefaultState);
			addState(mNormalState, mDefaultState);
			addState(mScanningState, mDefaultState);
			addState(mProcessingState, mDefaultState);

		setInitialState(mNormalState);
		
		alarmReceiver = new BroadcastReceiver() {
			@Override
			public void onReceive(Context context, Intent intent) {
				Log.i("ReadycastWakeup", "Rcvd MSG_ALARM: " + getCurrentState().getName());
				/* We need CPU lock to process the message */
				AcquireLock();
				sendMessage(MSG_ALARM, 0);
				return;
			}
		};
		ctx.registerReceiver(alarmReceiver, new IntentFilter(ACTION_DTP_ALARM));
		
		scanReceiver = new BroadcastReceiver() {
			@Override
			public void onReceive(Context context, Intent intent) {
				Log.i("ReadycastWakeup", "Rcvd SCAN_RESULTS_AVAILABLE_ACTION: " + getCurrentState().getName());
				last_scan = System.currentTimeMillis();
				sendMessage(MSG_SCAN_RESULTS_AVAILABLE, 0);
				return;
			}
		};
		ctx.registerReceiver(scanReceiver, new IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION));
		
		connectivityReceiver = new BroadcastReceiver() {
			@Override
			public void onReceive(Context context, Intent intent) {
				@SuppressWarnings("deprecation")
				NetworkInfo mNetworkInfo = intent.getParcelableExtra(ConnectivityManager.EXTRA_NETWORK_INFO);
				if (mNetworkInfo==null)
					return;
				if (mNetworkInfo.getType() == ConnectivityManager.TYPE_WIFI && 
						mNetworkInfo.getState() == NetworkInfo.State.DISCONNECTED) {
					Log.i("ReadycastWakeup", "Rcvd WIFI DISCONNECTED");
					/* Set last_scat as -1 to force scanning */
					last_scan = -1;
					sendMessage(MSG_ALARM, 0);
				}
				return;
			}
		};
		ctx.registerReceiver(connectivityReceiver, new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION));

		
		Log.i("ReadycastWakeup", "DTPWakeupStateMachine: constructed");
	}
	
	public void run () {
		Log.i("ReadycastWakeup", "DTPWakeupStateMachine: run!");
		ctx.sendBroadcast(new Intent(ACTION_DTP_ALARM));
	}
	
	class DefaultState extends State {
		@Override
		public void enter () {
			Log.i("ReadycastWakeup", "StateMachine: Enter DefaultState");
		}
		@Override
		public boolean processMessage (Message message) {
			Log.i("ReadycastWakeup", "DefaultState: Processing " + Integer.toString(message.what));
			switch (message.what) {
			case MSG_SCAN_RESULTS_AVAILABLE:
			case MSG_ALARM:
			case MSG_TIMEOUT:
			default:
				break;
			}
			/* DefaultState should always return true */
			return true;
		}
	}
	
	class NormalState extends State {
		@Override
		public void enter () {
			Log.i("ReadycastWakeup", "StateMachine: Enter NormalState");
		}
		@Override
		public boolean processMessage (Message message) {
			Log.i("ReadycastWakeup", "NormalState: Processing " + Integer.toString(message.what));
			switch (message.what) {
			case MSG_ALARM:
				transitionTo(mScanningState);
				return true;

			case MSG_SCAN_RESULTS_AVAILABLE:
			case MSG_TIMEOUT:
			default:
				return false;
			}
		}
	}
	
	class ScanningState extends State {
		@Override
		public void enter () {
			Log.i("ReadycastWakeup", "StateMachine: Enter ScanningState");

			/* Make sure we have CPU lock */
			AcquireLock();
			
			connectedAPInfo = wm.getConnectionInfo();
			if (connectedAPInfo.getBSSID() == null
					&& System.currentTimeMillis() > last_scan + MIN_SCAN_INTERVAL_MS) {
				if (wm.startScan() == false) {
					/* Failed scanning. So skip it */
					transitionTo(mProcessingState);
				}
				else
					sendMessageDelayed(MSG_TIMEOUT, SCAN_TIMEOUT_MS);
			}
			else
				transitionTo(mProcessingState);
		}
		@Override
		public boolean processMessage (Message message) {
			Log.i("ReadycastWakeup", "ScanningState: Processing " + Integer.toString(message.what));
			switch (message.what) {
			case MSG_SCAN_RESULTS_AVAILABLE:
			case MSG_TIMEOUT:
				transitionTo(mProcessingState);
				return true;
				
			case MSG_ALARM:
			default:
				return false;
			}
		}
	}
	
	class ProcessingState extends State {
		@Override
		public void enter () {
			int next;
			Log.i("ReadycastWakeup", "StateMachine: Enter ProcessingState");
			next = DTPNetThread.dtpgetlocktime();
			Log.i("ReadycastWakeup", "dtpgetlocktime() " + Integer.toString(next));

			long current_time = System.currentTimeMillis();
			long time = current_time + (next > 0 ? next : (-1 * next)) * 1000;

			if (comming_alarm < current_time	// No registered alarm 
					|| time < comming_alarm){	// New alarm is earlier than registered alarm
				CancelAlarm();
				SetAlarm(time);
			}

			if (next < 0) {
				ReleaseLock();
			}
			transitionTo(mNormalState);
		}
		@Override
		public boolean processMessage (Message message) {
			Log.i("ReadycastWakeup", "ProcessingState: Processing " + Integer.toString(message.what));
			switch (message.what) {
			case MSG_ALARM:				
			case MSG_SCAN_RESULTS_AVAILABLE:
			case MSG_TIMEOUT:
			default:
				return false;
			}
		}
	}
	
	public void SetAlarm(long time)
	{
		Log.i("ReadycastWakeup", "SetAlarm called! " + Long.toString(time));

	    Intent i = new Intent(ACTION_DTP_ALARM);
	    PendingIntent pi = PendingIntent.getBroadcast(ctx, 0, i, 0);
//	    am.setRepeating(AlarmManager.RTC_WAKEUP, System.currentTimeMillis(), 1000 * 60 * 10, pi); // Millisec * Second * Minute
	    am.set(AlarmManager.RTC_WAKEUP, time, pi);
	    comming_alarm = time;
	}
	
	public void CancelAlarm()
	{
		Log.i("ReadycastWakeup", "Alarm canceled");
	    Intent intent = new Intent(ctx, DTPNetAlarm.class);
	    PendingIntent sender = PendingIntent.getBroadcast(ctx, 0, intent, 0);
	    am.cancel(sender);
	}
	
	public void AcquireLock()
	{
		if (wl.isHeld() == false) {
			Log.i("ReadycastWakeup", "Acquire wakelock");
			wl.acquire();
		}
	}
	public void ReleaseLock()
	{
		if (wl.isHeld() == true) {
			Log.i("ReadycastWakeup", "Release wakelock");
			wl.release();
		}
	}
}
