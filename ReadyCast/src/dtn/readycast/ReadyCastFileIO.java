package dtn.readycast;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;

import org.json.JSONArray;
import org.json.JSONException;

import dtn.readycast.item.RSSFeed;
import dtn.readycast.item.RSSItem;
import dtn.readycast.ui.MainActivity;

import android.os.Environment;
import android.util.Log;

public class ReadyCastFileIO {

	public static String mSdPath = Environment.getExternalStorageDirectory().getAbsolutePath();

	
    public static void saveToFile(ArrayList<RSSFeed> feedlist) {
    	for (int i = 0; i < feedlist.size(); i++)
    		saveFeedToFile(feedlist.get(i));
    	saveFeedListToFile(feedlist);
    }
    
    public static void saveFeedListToFile(ArrayList<RSSFeed> feedlist) {
    	JSONArray feedArray = new JSONArray();
    	for (int i = 0; i < feedlist.size(); i++)
    		feedArray.put(feedlist.get(i).toJSONObject());
    	
    	try {
			FileWriter feeds_file = new FileWriter(mSdPath + "/ReadyCast/" + "/feeds.json");
	    	feeds_file.write(feedArray.toString());
	    	feeds_file.flush();
	    	feeds_file.close();
	    	
		} catch (IOException e) {
			e.printStackTrace();
		}
    }
	/*-------------------------------------------------------------------------*/

    public static void saveFeedToFile(RSSFeed feed) {
    	JSONArray itemArray = new JSONArray();
    	for (int j = 0; j < feed.items.size(); j++)
    		itemArray.put(feed.items.get(j).toJSONObject());

	    try {
	    	FileWriter items_file = new FileWriter(mSdPath + "/ReadyCast/" + feed.title + "/items.json");
			items_file.write(itemArray.toString());
			items_file.flush();
			items_file.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
    }
    
    public static void removeFeedFromFile(RSSFeed feed) {
	    try {
	    	File items_file = new File(mSdPath + "/ReadyCast/" + feed.title + "/items.json");
	    	items_file.delete();
		} catch (Exception e) {
			e.printStackTrace();
		}
    }
    
    public static void loadFromFile(MainActivity m, ArrayList<RSSFeed> feedlist) {
    	char[] buf = new char [32000];
    	String input = "";
		FileReader feeds_file;
		try {
			feeds_file = new FileReader(mSdPath + "/ReadyCast/" + "/feeds.json");
        	while (true) {
				if (feeds_file.read(buf) == -1)
					break;
        		input += new String(buf);
        	}
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}

		try {
			JSONArray feedArray = new JSONArray (input);
			for (int i = 0; i < feedArray.length(); i++) {
				/* 1) Create new feed */
				RSSFeed feed = new RSSFeed(feedArray.getJSONObject(i));
				
				/* 2) Add items to feed */
	        	String input_item = "";
				FileReader items_file;
				try {
					items_file = new FileReader(mSdPath + "/ReadyCast/" + feed.title + "/items.json");
		        	while (true) {
						if (items_file.read(buf) == -1)
							break;
						input_item += new String(buf);
		        	}
				} catch (FileNotFoundException e) {
					e.printStackTrace();
				} catch (IOException e) {
					e.printStackTrace();
				}

				JSONArray itemArray = new JSONArray(input_item);
				feed.items = new ArrayList<RSSItem> ();
				for (int j = 0; j < itemArray.length(); j++) {
					RSSItem data = new RSSItem(itemArray.getJSONObject(j));
					data.parentFeed = feed;
					feed.items.add(data);
					
					if (data.status == RSSItem.DOWNLOADED || data.status == RSSItem.DOWNLOADED_PLAYED) {
						File file = new File(mSdPath + "/ReadyCast/" + data.file_name);
						if (!file.exists()) {
							Log.d("readycast", "RESET!!!!! (filename = " + data.file_name + ")");
							data.enclosure_len = 0;
							if (data.status == RSSItem.DOWNLOADED || data.status == RSSItem.DOWNLOADED_PLAYED) {
								data.status = RSSItem.NONE;
								if (m != null)
									m.updateRecent();
							}
						}
						else {
							long file_len = file.length();
							if (data.enclosure_len > 0)
								data.percent = (int) ((file_len * 100) / data.enclosure_len);

							if (data.percent != 100) {
								data.status = RSSItem.DOWNLOADING;
								if (m != null)
									m.updateRecent();
							}
						}
					}
				}
				
				/* 3) Add feed to feed list */
				feedlist.add(feed);
        	}
		} catch (JSONException e) {
			e.printStackTrace();
		}
		
    }
    
	/*-------------------------------------------------------------------------*/



}
