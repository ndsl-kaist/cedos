package dtn.readycast.item;
import java.net.URL;
import java.util.ArrayList;

import javax.xml.parsers.SAXParserFactory;

import org.xml.sax.InputSource;
import org.xml.sax.XMLReader;

import android.os.AsyncTask;
import android.util.Log;
import dtn.readycast.ReadyCastFileIO;
import dtn.readycast.rss.RSSiTunesURLParser;
import dtn.readycast.rss.XMLParser;

/* derived class of AsyncTask to retrieve RSS feed from server */
public class RSSRetrieveFeedsInit extends AsyncTask<String, Void, String>
{
	private ArrayList<RSSFeed> list = null;
	private int sched_time;
	
	/* constructor for RetrieveRSSFeeds */
	public RSSRetrieveFeedsInit(int _sched_time) {
		sched_time = _sched_time;
		list = new ArrayList<RSSFeed>();
		ReadyCastFileIO.loadFromFile(null, list);
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
		/* if it has a parameter, it should retrieve the feed and add to original list
		 * (this is called when new feed address is added) */
		String msg = "";
		boolean exists = false;
		RSSFeed feed = retrieveRSSFeed(params[0]);
		if (feed != null && feed.title != null) {
			for (int i = 0; i < list.size(); i++) {
				if (list.get(i).title.compareTo(feed.title) == 0) {
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
				list.add(0, feed);
				msg = "New feed '" + feed.title + "' is added.";
				/* save new feed information into file */
				ReadyCastFileIO.saveFeedToFile(feed);
				/* update feedlist */
				ReadyCastFileIO.saveFeedListToFile(list);
			}
		}
		else {
			msg = "Cannot add a new feed! Disconnected network or wrong RSS feed.";
		}

		return msg;
	}

	@Override
	protected void onCancelled() {
		super.onCancelled();

	}

	@Override
	protected void onPreExecute() {
		super.onPreExecute();
	}

	@Override
	protected void onPostExecute(String print) {		
		super.onPostExecute(print);
	}
	
	@Override
	protected void onProgressUpdate(Void... values) {
		super.onProgressUpdate(values);
	}

}