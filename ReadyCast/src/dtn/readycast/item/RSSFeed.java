package dtn.readycast.item;

import java.util.ArrayList;
import java.util.Locale;

import org.json.JSONException;
import org.json.JSONObject;

import dtn.readycast.R;

import android.content.Context;

public class RSSFeed {
	/* required attributes for RSS feeds */
	public String url = null;
	public String title = null;
	public String link = null;
	public String description = null;
	public String itunes_id = null;
	public int sched_time = -1; // in minutes

	public ArrayList<RSSItem> items = null;

	public RSSFeed (JSONObject obj) {
		super();		
		if (obj == null)
			return;
		try {
			title 				= (String) obj.get("title");
			description			= (String) obj.get("description");
			link				= (String) obj.get("link");
			url					= (String) obj.get("url");
			itunes_id			= (String) obj.get("id");
			sched_time			= (Integer) obj.get("sched_time");
		} catch (JSONException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	public JSONObject toJSONObject() {
		JSONObject items = new JSONObject();
		try {
			items.put("title",				title);
			items.put("description",		description);
			items.put("link",				link);
			items.put("url",				url);
			items.put("id", 				itunes_id);
			items.put("sched_time",			sched_time);
		} catch (JSONException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return items;
	}

	public void setSchedule (int hour, int minute) {
		if (0 <= hour && hour < 24 && 0 <= minute && minute < 60)
			sched_time = hour * 60 + minute;
	}

	public String getScheduleText (Context c) {

		Locale current = c.getResources().getConfiguration().locale;

		if (sched_time == -1)
			return c.getString(R.string.tab1_manual);

		String time = "";
		if (current.getCountry().equalsIgnoreCase("KR")) {

			switch (sched_time / 60) {
			case 2: case 3: case 4: case 5: case 6:
				time += "새벽 ";
				break;
			case 7: case 8: case 9: case 10: case 11:
				time += "아침 ";
				break;
			case 12: case 13: case 14: case 15: case 16: case 17:
				time += "오후 ";
				break;
			case 18: case 19: case 20:
				time += "저녁 ";
				break;
			case 21: case 22: case 23: case 0: case 1:
				time += "밤 ";
				break;
			}
			int hour = sched_time / 60;
			if (hour > 12)	hour -= 12;
			if (hour == 0)	hour += 12;
			time = time + hour + "시";
			if (sched_time % 60 != 0)
				time = time + " " + sched_time % 60 + "분";
		}
		else {

			int hour = this.sched_time / 60;

			if (hour < 12)
				time = "AM ";
			else
				time = "PM ";

			if (hour > 12)	hour -= 12;
			if (hour == 0)	hour += 12;

			if (this.sched_time % 60 < 10)
				time = hour + ":0" + this.sched_time % 60 + time;
			else
				time = hour + ":" + this.sched_time % 60 + time;
		}

		return c.getString(R.string.tab1_sched_1) + " " + time + c.getString(R.string.tab1_sched_2);
	}
	
	public String getNumDownloadingText(Context c, boolean emptyIfNull) {
		String str;
		int count = getNumDownloading();
		if (emptyIfNull) {
			if (count == 0)
				str = "";
			else
				str = c.getString(R.string.tab1_num_down) + " " + getNumDownloading() + " "
						+ c.getString(R.string.tab1_num_down_2);		
		}
		else {
			str = c.getString(R.string.tab1_num2_down) + " " + getNumDownloading() + " "
					+ c.getString(R.string.tab1_num2_down_2);
		}
		return str;
	}
	
	public int getNumDownloading() {
		int index = 0;
		int count = 0;
		while (index < items.size()) {
			if (items.get(index).status == RSSItem.DOWNLOADING) {
				count++;
			}
			index++;
		}
		return count;
	}
	
	public int getNumNewDownloaded() {
		int index = 0;
		int count = 0;
		while (index < items.size()) {
			if (items.get(index).status == RSSItem.DOWNLOADED) {
				count++;
			}
			index++;
		}
		return count;
	}
}
