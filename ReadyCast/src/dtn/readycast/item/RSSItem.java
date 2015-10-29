package dtn.readycast.item;

import org.json.JSONException;
import org.json.JSONObject;

import android.os.Bundle;
import android.os.Environment;
import android.os.Message;
import android.os.RemoteException;
import android.util.Log;
import dtn.net.service.IDownloadCallback;
import dtn.readycast.ReadyCastFileIO;
import dtn.readycast.ui.Tab1_Fragment;
import dtn.readycast.ui.MainActivity;
public class RSSItem {
	/* required attributes for RSS items */
	public String title = "";         /* title */
	public String description = "";   /* description */
	
	/* optional attributes for RSS items */
	public String pubDate_str;         		/* published date in String */
	public long pubDate;
	public String link = "";          			/* URL of website corresponding to the item  */
	public String enclosure_url = ""; 	/* URL of attached media object */
	public long enclosure_len = 0;
	public int percent = 0;
	public long deadline_sec = 0;
	public String uuid = "";
	
	public static final int NONE        	  = 0;
	public static final int NOTFOUND  		  = 1;
	public static final int DOWNLOADING 	  = 2;
	public static final int DOWNLOADED  	  = 3;
	public static final int DOWNLOADED_PLAYED = 4;
	
	public int status = NONE;
	
	public String file_name = "";
	public String file_type = null;

	public RSSFeed parentFeed = null;
	
	public RSSItem (JSONObject obj) {
		super();
		if (obj == null)
			return;
		try {
			title 				= (String) obj.get("title");
			description			= (String) obj.get("description");
			pubDate_str 		= (String) obj.get("pubDate_str");
			link				= (String) obj.get("link");
			enclosure_url		= (String) obj.get("enclosure_url");
			enclosure_len		= (Integer) obj.get("enclosure_len");
			deadline_sec		= (Integer) obj.get("deadline_sec");
			status              = (Integer) obj.get("status");
			file_name			= (String) obj.get("file_name");
			uuid			= (String) obj.get("uuid");
		} catch (JSONException e) {
			// TODO Auto-generated catch block
			Log.e("System.err", e.getMessage() + "\n" + obj.toString());
			e.printStackTrace();
		}
	}
	
	public JSONObject toJSONObject() {
    	JSONObject items = new JSONObject();
		try {
			items.put("title",				title);
			items.put("description",		description);
			items.put("pubDate",			pubDate);
			items.put("pubDate_str",		pubDate_str);
			items.put("link",				link);
			items.put("enclosure_url",		enclosure_url);
			items.put("enclosure_len",		enclosure_len);
			items.put("deadline_sec", 	deadline_sec);
			items.put("status",             status);
			items.put("file_name",			file_name);
			items.put("uuid",			uuid);
		} catch (JSONException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return items;
	}
	
	public void initCallback() {
		mCallback = new IDownloadCallback.Stub() {
			public void setContentLength(long contentLength) {
				enclosure_len = contentLength;
				ReadyCastFileIO.saveFeedToFile(parentFeed);
			}
			
	        public void valueChanged(int value) {
				
				percent = value;
				Message a = Tab1_Fragment.handle.obtainMessage();
				a.what = Tab1_Fragment.STAT_UPDATED;
				Tab1_Fragment.handle.sendMessage(a);
				
				
				if (percent == 100) {
					status = DOWNLOADED;
					Message b = MainActivity.main_handle.obtainMessage();
					b.what = MainActivity.CONN_CLOSED;
					Bundle data = new Bundle();
					data.putString("file_name", file_name);
					data.putString("file_type", file_type);
					data.putString("title", title);
					b.setData(data);
					MainActivity.main_handle.sendMessage(b);
				}
	        }

			public void setNotFound() {
				Log.d("libdtp", "not found!");
				percent = 0;
				status = RSSItem.NOTFOUND;
				Message a = Tab1_Fragment.handle.obtainMessage();
				a.what = Tab1_Fragment.STAT_UPDATED;
				Tab1_Fragment.handle.sendMessage(a);

				Message b = MainActivity.main_handle.obtainMessage();
				b.what = MainActivity.LIST_UPDATED;
				MainActivity.main_handle.sendMessage(b);
				
				ReadyCastFileIO.saveFeedToFile(parentFeed);				
			}
		};
	}
	
	public boolean startDownload (MainActivity main_activity) {
		if (enclosure_url == null) {
			return false;
		}

		initCallback();
		try {
			uuid = main_activity.mService.registerDownload(mCallback, enclosure_url, 
						Environment.getExternalStorageDirectory().getAbsolutePath() + "/ReadyCast/" + file_name,
						(int) (deadline_sec - (System.currentTimeMillis() / 1000)));
		} catch (RemoteException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (NullPointerException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		status = RSSItem.DOWNLOADING;
		main_activity.updateRecent();
		
		return true;
	}
	
    /**
     * callback interface
     */
    public IDownloadCallback mCallback = null;
}

