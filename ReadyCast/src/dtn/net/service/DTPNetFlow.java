package dtn.net.service;

import java.io.BufferedOutputStream;
import java.io.File;
import java.util.UUID;

import org.json.JSONException;
import org.json.JSONObject;

public class DTPNetFlow {

	/*------------------------------------*/
	public IDownloadCallback callback = null;
	public long initialSec;
	public String url;
	public String file_path;
	public long deadlineDueSec;	
	public long startTime;
	public String uuid;
	public String partialContentIndex = "-";
	/*------------------------------------*/
	public int status = 0;
	// 0 ; ongoing download
	// 1 ; add new download
	// 2 ; cancel download
	/*------------------------------------*/
	public int prev_percent;
	public int socket = 0;
	public long total_len = 0; // total length (prev file size + content length)
	public long file_len = 0; //  recv len plus offset (offset = prev file size)
	public long down_len = 0; // recv len

	public String header = "";
	
	public File file = null;
	public BufferedOutputStream fout = null;
	public boolean isResponseParsed = false;
	public boolean isMalformedHeader = false;
	public int retry_num = 0;
	
	/*------------------------------------*/
	
	public DTPNetFlow(IDownloadCallback cb, String u, String f,
			int deadlineSec) {
		callback = cb;

		initialSec = System.currentTimeMillis() / 1000;
		url = u;
		file_path = f;
		deadlineDueSec = System.currentTimeMillis() / 1000 + deadlineSec;
		startTime = System.currentTimeMillis();
		
		uuid = UUID.randomUUID().toString();
	}

	public DTPNetFlow(JSONObject obj) {
		if (obj == null)
			return;
		try {
			uuid = (String) obj.getString("uuid");
			initialSec = (Integer) obj.get("init_time");
			url = (String) obj.get("url");
			file_path = (String) obj.get("file_path");
			deadlineDueSec = (Integer) obj.get("deadline");
			startTime = System.currentTimeMillis();
		} catch (JSONException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	public JSONObject toJSONObject() {
		JSONObject items = new JSONObject();
		try {
			items.put("uuid", uuid);
			items.put("init_time", initialSec);
			items.put("url", url);
			items.put("file_path", file_path);
			items.put("deadline", deadlineDueSec);

		} catch (JSONException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return items;
	}
	
	public int getTimeUntilDeadline() {
		int timeUntilDeadline = (int) (deadlineDueSec - System
				.currentTimeMillis() / 1000);
		if (timeUntilDeadline < 0)
			timeUntilDeadline = 0;

		return timeUntilDeadline;
	}


}
