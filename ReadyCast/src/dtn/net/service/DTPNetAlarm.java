package dtn.net.service;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.PowerManager;
import android.util.Log;

public class DTPNetAlarm extends BroadcastReceiver {
	PowerManager pm;
	WifiManager wm;
	PowerManager.WakeLock wl = null;
	WifiInfo connectedAPInfo=null;

	long comming_alarm = -1;
	long last_scan = -1;
	public static final String ACTION_DTP_ALARM = "dtn.net.service.Alarm";
	
	@Override
	public void onReceive(Context context, Intent intent) 
	{
		int next = 0;
		Log.i("Readycast", "Received Alarm");

		if (wl == null)
			return;
		
		if (wl.isHeld() == false) {
			Log.i("Readycast", "Acquire wakelock");
			wl.acquire();
		}
		
		connectedAPInfo = wm.getConnectionInfo();
		if (connectedAPInfo.getBSSID() == null) {
			if (wm.startScan()) {
			}
		}

		Log.i("Readycast", "Entering dtpgetlocktime()");
		next = DTPNetThread.dtpgetlocktime();
		Log.i("Readycast", "dtpgetlocktime() " + Integer.toString(next));

		long current_time = System.currentTimeMillis();
		long time = current_time + (next > 0 ? next : (-1 * next) * 1000);

		if (comming_alarm < current_time	// No registered alarm 
				|| time < comming_alarm){	// New alarm is earlier than registered alarm
			CancelAlarm(context);
			SetAlarm(context, time);
		}

		if (next < 0) {
			Log.i("Readycast", "Release wakelock");
			wl.release();
		}
	}
	
	public DTPNetAlarm(Context ctx) {
		Log.i("Readycast", "Alarm constructed");
		wm = (WifiManager)ctx.getSystemService(Context.WIFI_SERVICE);
		pm = (PowerManager)ctx.getSystemService(Context.POWER_SERVICE);
		
		wl = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "");
	}

	public void SetAlarm(Context context, long time)
	{
		Log.i("Readycast", "SetAlarm called! " + Long.toString(time));

	    AlarmManager am = (AlarmManager)context.getSystemService(Context.ALARM_SERVICE);
	    Intent i = new Intent(ACTION_DTP_ALARM);
	    PendingIntent pi = PendingIntent.getBroadcast(context, 0, i, 0);
//	    am.setRepeating(AlarmManager.RTC_WAKEUP, System.currentTimeMillis(), 1000 * 60 * 10, pi); // Millisec * Second * Minute
	    am.set(AlarmManager.RTC_WAKEUP, time, pi);
	    comming_alarm = time;
	}
	
	public void CancelAlarm(Context context)
	{
		Log.i("Readycast", "Alarm canceled");
	    Intent intent = new Intent(context, DTPNetAlarm.class);
	    PendingIntent sender = PendingIntent.getBroadcast(context, 0, intent, 0);
	    AlarmManager alarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
	    alarmManager.cancel(sender);
	}
}
