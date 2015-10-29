package dtn.readycast.rss;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;

public class RSSiTunesURLParser {

	public static final String ITUNES_AGENT = "iTunes/9.1.1";
	public static final String FEED_URL = "feed-url=\"";
	public static final String[] idstyles = {"?id=", "&id=", "/id"};
	public static final String[] formats = {
		"https://itunes.apple.com/podcast/id",
		"https://itunes.apple.com/WebObjects/DZR.woa/wa/viewPodcast?id="};
	
	public static String ExtractURL (String org_url) {
		int i;
		String extractedURL = "";
		String id = ExtractiTunesID(org_url);
		if (id == null)
			return "";
		
		for (i = 0; i < formats.length; i++) {
			String str = RetrieveURL(formats[i] + id);
			if (str != null) {
				int start, end;
				start = str.indexOf(FEED_URL);
				if (start == -1)
					continue;
				start += FEED_URL.length();
				end = str.indexOf("\"", start);
				extractedURL = str.substring(start, end);
				return extractedURL;
			}
		}
		return "";
	}
	
	public static String ExtractiTunesID (String url_in) {
		String res = "";
		int i;
		for (i = 0; i < idstyles.length; i++) {
			int start = url_in.lastIndexOf(idstyles[i]);
			if (start >= 0) {
				start += idstyles[i].length();
				
				while (start < url_in.length()) {
					if (Character.isDigit(url_in.charAt(start))) {
						res += url_in.charAt(start);
						start++;
					}
					else
						break;
				}
				return res;
			}
		}		
		return null;
	}
	
	public static String RetrieveURL (String url_in) {
		HttpURLConnection urlConnection = null;
		String str = new String();
		int bytes_read;
		
		byte[] buffer = new byte[32*1024];
		try {
			URL url = new URL(url_in);
			urlConnection = (HttpURLConnection) url.openConnection();
			urlConnection.setRequestProperty("User-Agent", ITUNES_AGENT);
			urlConnection.setInstanceFollowRedirects(true);
			urlConnection.connect();
			InputStream in = new BufferedInputStream(urlConnection.getInputStream());
			while (true) {
				if ((bytes_read = in.read(buffer)) == -1)
					break;
				str += new String(buffer, 0, bytes_read);
			}
		} catch (MalformedURLException e) {
			e.printStackTrace();
			str = null;
		} catch (IOException e) {
			e.printStackTrace();
			str = null;
		}
		finally {
			if (urlConnection != null)
				urlConnection.disconnect();
		}
		
		return str;
	}
}
