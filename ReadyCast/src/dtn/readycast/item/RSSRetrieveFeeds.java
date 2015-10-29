package dtn.readycast.item;
import java.io.File;
import java.net.URL;
import java.util.ArrayList;
import javax.xml.parsers.SAXParserFactory;
import org.xml.sax.InputSource;
import org.xml.sax.XMLReader;

import android.app.Activity;
import android.app.ProgressDialog;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.os.Message;
import android.os.RemoteException;
import android.util.Log;
import android.widget.Toast;
import dtn.readycast.ReadyCastFileIO;
import dtn.readycast.rss.RSSiTunesURLParser;
import dtn.readycast.rss.XMLParser;
import dtn.readycast.ui.MainActivity;

/* derived class of AsyncTask to retrieve RSS feed from server */
public class RSSRetrieveFeeds extends AsyncTask<String, Void, String>
{

	private MainActivity main_activity = null;
	private ProgressDialog progress = null;
	private int sched_time;

	/* constructor for RetrieveRSSFeeds */
	public RSSRetrieveFeeds(Activity _main_activity, int _sched_time) {
		if (_main_activity != null) {
			main_activity = (MainActivity) _main_activity;
		}
		sched_time = _sched_time;
	}

	/* calls XMLParser for retrieving the feed */
	private RSSFeed retrieveRSSFeed(String urlToRssFeed)
	{
		RSSFeed feed = new RSSFeed(null);

		if (urlToRssFeed.contains("itunes.apple.com")) {
			feed.itunes_id = RSSiTunesURLParser.ExtractiTunesID(urlToRssFeed);
			urlToRssFeed = RSSiTunesURLParser.ExtractURL(urlToRssFeed);
			if (urlToRssFeed == "")
				return null;
			Log.d("readycast", "iTunes redirected URL = " + urlToRssFeed);

			Log.d("readycast", "iTunes added id = " + feed.itunes_id);
		}

		try {
			feed.url = urlToRssFeed;
			feed.items = new ArrayList<RSSItem>();

			URL url = new URL(urlToRssFeed);
			SAXParserFactory factory = SAXParserFactory.newInstance();
			XMLReader xmlreader = factory.newSAXParser().getXMLReader();
			XMLParser theRssHandler = new XMLParser(feed);
			xmlreader.setContentHandler(theRssHandler);
			InputSource is = new InputSource(url.openStream());
			xmlreader.parse(is);
		}
		catch (Exception e) {
			e.printStackTrace();
		}
		return feed;
	}

	/* perform followings in background */
	protected String doInBackground(String... params) {
		String msg = "";
		/* no parameter means loading the whole feed */
		if (params[0] == "") {
			File file = new File(ReadyCastFileIO.mSdPath + "/ReadyCast/feeds.json");
			/* load feed from file */
			if (file.exists()) {
				ReadyCastFileIO.loadFromFile(main_activity, main_activity.feedlist);
			}
			else {
				/* XXX : fixed to remove default feed list
				String[] default_feedlist = main_activity.getResources().getStringArray(R.array.default_feeds);
				// load default feed address from arrays.xml
				for (int i = 0; i < default_feedlist.length; i++) {
					RSSFeed feed = retrieveRSSFeed(default_feedlist[i]);
					if (feed != null && feed.title != null)
						main_activity.feedlist.add(feed);
				}
				ReadyCastFileIO.saveToFile(main_activity.feedlist);
				 */
			}
		}
		/* if it has a parameter, it should retrieve the feed and add to original list
		 * (this is called when new feed address is added) */
		else {
			boolean exists = false;
			RSSFeed feed = retrieveRSSFeed(params[0]);
			if (feed != null && feed.title != null) {
				for (int i = 0; i < main_activity.feedlist.size(); i++) {
					if (main_activity.feedlist.get(i).title.compareTo(feed.title) == 0) {
						exists = true;
						break;
					}
				}
				if (exists) {
					msg = "The feed '" + feed.title + "' already exists.";
				}
				else {
					if (sched_time >= 0) {
						feed.sched_time = sched_time;
					}
					main_activity.feedlist.add(0, feed);
					msg = "New feed '" + feed.title + "' is added.";
					/* save new feed information into file */
					ReadyCastFileIO.saveFeedToFile(feed);
					/* update feedlist */
					ReadyCastFileIO.saveFeedListToFile(main_activity.feedlist);
				}
			}
			else {
				msg = "Cannot add a new feed! Disconnected network or wrong RSS feed.";
			}
		}

		return msg;
	}

	@Override
	protected void onCancelled() {
		super.onCancelled();		
	}

	@Override
	protected void onPreExecute() {
		/* XXX : blocks the user until the loading completes. must be changed to non-blocking. */
		try {
			progress = ProgressDialog.show(main_activity, null, "Loading RSS Feeds...");
		}
		catch (RuntimeException e) {
			if (main_activity == null)
				Log.d("appdtp", "main_act is null");
			else
				Log.d("appdtp", main_activity.toString());
		}
		super.onPreExecute();
	}

	/* after retrieval completes,
	 * 1) it creates the adapter and handler for list,
	 * 2) and restart the download automatically */
	@Override
	protected void onPostExecute(String print) {
		//RSSListFragment main_fragment = null;
		main_activity.current_feed = 0;

		if (progress != null && progress.isShowing())
			progress.dismiss();

		if (print != "")
			Toast.makeText(main_activity, print, Toast.LENGTH_SHORT).show();

		main_activity.createHandler();

		Log.d("readycast", "connect to Downloads");

		for (int i = 0; i < main_activity.feedlist.size(); i++) {
			int updated = 0;
			for (int j = 0; j < main_activity.feedlist.get(i).items.size(); j++) {
				RSSItem data = main_activity.feedlist.get(i).items.get(j);
				if (data.status == RSSItem.DOWNLOADING) {
					// restart download automatically
					try {
						data.initCallback();
						if (data.mCallback != null && main_activity.mService != null) {
							int ret = main_activity.mService.connectToDownload(data.mCallback, data.uuid);
							if (ret > 0) {
								Log.d("app_debug", "ret = " + ret);
								data.percent = -2;
							}
							else {
								File file = new File(Environment.getExternalStorageDirectory().getAbsolutePath() + "/ReadyCast/" + data.file_name);								
								if (data.enclosure_len == file.length()) {
									data.status = RSSItem.DOWNLOADED;		
									/* notify on the title bar */
									Message b = MainActivity.main_handle.obtainMessage();
									b.what = MainActivity.CONN_CLOSED;
									Bundle d = new Bundle();
									d.putString("file_name", data.file_name);
									d.putString("file_type", data.file_type);
									d.putString("title", data.title);
									b.setData(d);
									MainActivity.main_handle.sendMessage(b);
								}
								else
									data.status = RSSItem.NONE;
								updated = 1;
							}
						}
					} catch (RemoteException e) {
						e.printStackTrace();
					}
				}
			}
			if (updated == 1) {
				ReadyCastFileIO.saveFeedToFile(main_activity.feedlist.get(i));
				Message b = MainActivity.main_handle.obtainMessage();
				b.what = MainActivity.LIST_UPDATED;
				MainActivity.main_handle.sendMessage(b);
			}
				
				
		}


		main_activity.updateRecent();
		main_activity.refreshCurrentTab();
		new RSSUpdateFeeds(main_activity).execute("");
		super.onPostExecute(print);
	}
	@Override
	protected void onProgressUpdate(Void... values) {
		super.onProgressUpdate(values);
	}

}