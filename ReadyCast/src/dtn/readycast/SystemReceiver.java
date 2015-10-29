package dtn.readycast;

import dtn.readycast.ui.MainActivity;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Message;
import android.util.Log;

public class SystemReceiver extends BroadcastReceiver {
	@Override
	public void onReceive(Context context, Intent intent) {    
		
		
		if (intent.getAction().equals("android.intent.action.BOOT_COMPLETED")) {
			ComponentName svcName;
			svcName = context.startService(new Intent("dtn.net.service.DTPDownloadService"));
			if (svcName == null)
				Log.d("readycast", "SystemReceiver : service not found");
			
		}

		if (intent.getAction().equals("android.intent.action.ACTION_SHUTDOWN")) {
			// DTPNetLogger.saveStatusToFile();
			if (MainActivity.main_handle != null) {
				Message b = MainActivity.main_handle.obtainMessage();
				b.what = MainActivity.SYSTEM_SHUTDOWN;
				MainActivity.main_handle.sendMessage(b);
			}
		}
	}
}