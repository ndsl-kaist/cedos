package dtn.readycast.item;
import java.net.URL;
import javax.xml.parsers.SAXParserFactory;
import org.xml.sax.InputSource;
import org.xml.sax.XMLReader;
import android.app.ProgressDialog;
import android.os.AsyncTask;
import android.widget.Toast;
import dtn.readycast.ReadyCastFileIO;
import dtn.readycast.rss.XMLRefresh;
import dtn.readycast.ui.MainActivity;

/* derived class of AsyncTask to retrieve RSS feed from server */
public class RSSUpdateFeeds extends AsyncTask<String, Void, String>
{
	
	private MainActivity main_activity = null;
	private ProgressDialog progress = null;
	
	/* constructor for RetrieveRSSFeeds */
	public RSSUpdateFeeds(MainActivity _main_activity) {
		main_activity = _main_activity;
	}
	
	/* calls XMLParser for retrieving the feed */
	private RSSFeed updateRSSFeed(RSSFeed feed)
	{
		try {
			URL url = new URL(feed.url);
			SAXParserFactory factory = SAXParserFactory.newInstance();
			XMLReader xmlreader = factory.newSAXParser().getXMLReader();
			XMLRefresh theRssHandler = new XMLRefresh(feed);
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
		/* no parameter means updating the whole feed */
		if (params[0] == "") {
			
			for (int i = 0; i < main_activity.feedlist.size(); i++)
				updateRSSFeed(main_activity.feedlist.get(i));

			ReadyCastFileIO.saveToFile(main_activity.feedlist);
		}
		/* if it has a parameter, it should update the feed and add to original list
		 * (this is called when new feed address is added) */
		else {

		}
		
		return msg;
	}

	@Override
	protected void onCancelled() {
		super.onCancelled();
	}

	@Override
	protected void onPreExecute() {
		//progress = ProgressDialog.show(main_activity, null, "Refreshing the feeds...");
		super.onPreExecute();
	}

	@Override
	protected void onPostExecute(String print) {
		if (progress != null && progress.isShowing())
			progress.dismiss();

		if (print != "")
			Toast.makeText(main_activity, print, Toast.LENGTH_SHORT).show();
		
		// ASDFASDF
		//((RSSListFragment)main_activity.current_fragment).uiRefresh();

		super.onPostExecute(print);
	}
	@Override
	protected void onProgressUpdate(Void... values) {
		super.onProgressUpdate(values);
	}
	
}