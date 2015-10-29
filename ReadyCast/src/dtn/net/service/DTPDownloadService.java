package dtn.net.service;

import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;

import org.json.JSONArray;
import org.json.JSONException;
import dtn.net.service.DTPDownloadInterface;
import dtn.net.service.IDownloadCallback;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.wifi.WifiManager;
import android.os.Environment;
import android.os.IBinder;
import android.os.PowerManager;
import android.os.RemoteException;
import android.telephony.TelephonyManager;
import android.util.Log;

public class DTPDownloadService extends Service
{	
	public static String mSdPath = Environment.getExternalStorageDirectory().getAbsolutePath();
	public static ArrayList<DTPNetFlow> downloadQueue = null;
	public DTPNetThread th = null;
	WifiManager wm;
	PowerManager powerMgr;
	PowerManager.WakeLock cpuLock;
	DTPNetAlarm alarm;
	String deviceid;
	private Context ctx;

	BroadcastReceiver receiver = new BroadcastReceiver() {
		@Override
		public void onReceive(Context context, Intent intent) {
			Log.d("readycast", "onreceive SHUTDOWN");			
		}
	};
	IntentFilter filter = new IntentFilter();
	IntentFilter filter2 = new IntentFilter();
	IntentFilter filter3 = new IntentFilter();
	@Override
	public void onCreate()
	{		
		TelephonyManager mngr = (TelephonyManager) (getSystemService(Context.TELEPHONY_SERVICE));
		deviceid = mngr.getDeviceId();

		Log.d("readycast", "DTPDownloadService() : onCreate()");
		filter.addAction("android.intent.action.ACTION_SHUTDOWN");
		registerReceiver(receiver, filter);

		filter2.addAction(ConnectivityManager.CONNECTIVITY_ACTION);

		wm = (WifiManager)this.getSystemService(Context.WIFI_SERVICE);
		powerMgr = (PowerManager)this.getSystemService(Context.POWER_SERVICE);

		/* Start periodic CPU wakeup */
		ctx = getApplicationContext();
		DTPWakeupStateMachine wSm = new DTPWakeupStateMachine(ctx);
		wSm.start();
		wSm.run();
		
		/*
		alarm = new DTPNetAlarm(ctx);
		filter3.addAction(DTPNetAlarm.ACTION_DTP_ALARM);
		registerReceiver(alarm, filter3);
		ctx.sendBroadcast(new Intent(DTPNetAlarm.ACTION_DTP_ALARM));
		 */
		
		// if it restarted after crash or started on boot, load the download list
		if (downloadQueue == null)
			loadDownloadListFromFile();

		super.onCreate();
	}

	@Override
	public void onDestroy()
	{
		Log.d("readycast", "DTPDownloadService() : onDestroy()");
		unregisterReceiver(receiver);
		super.onDestroy();
	}

	@Override
	public int onStartCommand(Intent Intent, int Flags, int StartId)
	{
		Log.d("readycast", "DTPDownloadService() : onStartCommand()");

		if (Intent == null)
			Log.d("readycast", "Intent : null");
		else if (Intent.getAction() == null)
			Log.d("readycast", "Intent.getAction : null");
		else
			Log.d("readycast", Intent.getAction());		
		Log.d("readycast", "reload downQueue");

		super.onStartCommand(Intent, Flags, StartId);

		return START_STICKY;
	}

	/*-------------------------------------------------------------------------*/

	private final DTPDownloadInterface.Stub mBinder = new DTPDownloadInterface.Stub()
	{
		@Override
		public String registerDownload(IDownloadCallback cb, String url, String filepath, int deadlineSec) throws RemoteException
		{
			Log.d("dtp_net", "registerDownload() : adding a new download");
			DTPNetFlow f = new DTPNetFlow(cb, url, filepath, deadlineSec);
			downloadQueue.add(f);
			f.status = 1;

			if (th == null || !th.isAlive()) {
				th = new DTPNetThread(getApplicationContext(), downloadQueue, deviceid, wm, powerMgr);
				th.start();
			}

			Log.i("dtp_net", "downlist count = " + downloadQueue.size());
			saveDownloadListToFile();

			return f.uuid;
		}

		@Override
		public int connectToDownload(IDownloadCallback cb, String _uuid) throws RemoteException
		{		
			if (downloadQueue == null)
				loadDownloadListFromFile();

			if (_uuid == "") {
				Log.d("readycast", "empty uuid");
				return -1;
			}
			if (_uuid == null) {
				Log.d("readycast", "null uuid");
				return -1;
			}	

			Log.d("readycast", "connectToDownload start" + downloadQueue.size());


			// if download request with same url before, just connect callback function
			for (int i = 0; i < downloadQueue.size(); i++) {
				String downloadingUUID = downloadQueue.get(i).uuid;
				if (_uuid.equals(downloadingUUID)) {
					Log.d("app_debug", "connectToDownload() : connects to previous download " + _uuid + " percent" +  downloadQueue.get(i).prev_percent);
					downloadQueue.get(i).callback = cb;
					return 1;
				}
				else
					Log.d("readycast", "not matching uuid" + _uuid + " and " + downloadingUUID);
			}

			Log.d("readycast", "connectToDownload failed");

			return -1;
		}

		@Override
		public void unregisterDownload(String _uuid) throws RemoteException
		{
			Log.d("readycast", "cancelDownload() start : " + _uuid);

			// cancels the download
			for (int i = 0; i < downloadQueue.size(); i++) {
				String downloadingUUID = downloadQueue.get(i).uuid;
				if (_uuid.equals(downloadingUUID)) {
					Log.d("readycast", "cancelDownload() : cancels download " + _uuid);	
					downloadQueue.get(i).status = 2; // cancel the download
					return;
				}
			}
		}

		@Override
		public void finishDownloads() throws RemoteException
		{
			// cancels the download		
			Log.d("readycast", "finishDownloads() called");

			for (int i = 0; i < downloadQueue.size(); i++) {
				// downloadQueue.get(i).interrupt();
			}
			saveDownloadListToFile();		
		}


		@Override
		public void systemShutdown() throws RemoteException {
			if (th != null)
				th.setInterrupt = true;
		}

	};

	/*-------------------------------------------------------------------------*/


	public static void saveDownloadListToFile () {

		JSONArray downlistArray = new JSONArray();
		for (int i = 0; i < downloadQueue.size(); i++)
			downlistArray.put(downloadQueue.get(i).toJSONObject());

		try {
			FileWriter downlist_file = new FileWriter(mSdPath + "/ReadyCast/downlist.json");
			downlist_file.write(downlistArray.toString());
			downlist_file.flush();
			downlist_file.close();

		} catch (IOException e) {
			e.printStackTrace();
		}


	}

	public void loadDownloadListFromFile() {
		Log.d("readycast", "loadDownloadListFromFile start");
		downloadQueue = new ArrayList<DTPNetFlow>();

		char[] buf = new char [32000];
		String input = "";
		FileReader downlist_file;
		try {
			downlist_file = new FileReader(mSdPath + "/ReadyCast/downlist.json");
			while (true) {
				if (downlist_file.read(buf) == -1)
					break;
				input += new String(buf);
			}
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
		try {
			JSONArray downlistArray = new JSONArray (input);
			for (int i = 0; i < downlistArray.length(); i++) {
				DTPNetFlow th = new DTPNetFlow(downlistArray.getJSONObject(i));
				downloadQueue.add(th);
			}
		} catch (JSONException e) {
			e.printStackTrace();
		}

		/* XXX: deprecated
		buf = new char [32000];
		input = "";
		try {
			downlist_file = new FileReader(mSdPath + "/ReadyCast/complist.json");
			while (true) {
				if (downlist_file.read(buf) == -1)
					break;
				input += new String(buf);
			}
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
		 */

		// start the loaded downloads
		for (int i = 0; i < downloadQueue.size(); i++) {
			DTPNetFlow f = downloadQueue.get(i);
			f.status = 1;
		}
		if (downloadQueue.size() > 0) {
			if (th == null || !th.isAlive()) {
				th = new DTPNetThread(getApplicationContext(), downloadQueue, deviceid, wm, powerMgr);
				th.start();
			}
		}

		Log.d("readycast", "loadDownloadListFromFile end");
	}

	/*-------------------------------------------------------------------------*/
	@Override
	public IBinder onBind(Intent intent) {
		return mBinder;
	}
	@Override
	public boolean onUnbind(Intent intent) {
		return false;
	}

}
