package dtn.readycast.rss;

import java.io.File;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

import org.xml.sax.Attributes;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;

import dtn.readycast.ReadyCastFileIO;
import dtn.readycast.item.RSSFeed;
import dtn.readycast.item.RSSItem;

public class XMLParser extends DefaultHandler {
	private final static String TAG_ITEM = "item";
	private final static String[] xmltags = {"title", "link", "pubDate", "description", "enclosure", "length"};
	private RSSItem currentitem = null;
	private RSSFeed currentfeed = null;
	private int currentindex = -1;
	private boolean isParsing = false;
	private StringBuilder builder;
	private SimpleDateFormat readFormat, readFormat2, printFormat;
	
	public XMLParser(RSSFeed feed) {
		super();
		this.currentfeed = feed;
		builder = new StringBuilder();
		readFormat = new SimpleDateFormat ("EEE, dd MMM yyyy HH:mm:ss Z", Locale.US);
		readFormat2 = new SimpleDateFormat ("EEE, dd MMM yyyy HH:mm Z", Locale.US);
		printFormat = new SimpleDateFormat ("yyyy-MM-dd (EEE) HH:mm:ss", Locale.US);
	}

	@Override
	public void characters(char[] ch, int start, int length) throws SAXException {
		super.characters(ch, start, length);
		if (currentindex != -1 && builder != null)
			builder.append(ch,start,length);
	}
		
	@Override
	public void startElement(String uri, String localName, String qName, Attributes attributes) throws SAXException {
		super.startElement(uri, localName, qName, attributes);

		/* <item> */
		if (localName.equalsIgnoreCase(TAG_ITEM))
		{
			currentitem = new RSSItem(null);
			currentindex = -1;
			isParsing = true;
		}
		else {
			currentindex = itemIndexFromString(localName);
			builder = null;
			if (currentindex != -1)
				builder = new StringBuilder();
			
			/* 
			 * Subfields should be parsed separately.
			 * (i.e.) <enclosure url="url/filename.mp3" length="1024" type="audio/mpeg"/>
			 */
			if (localName.equalsIgnoreCase("enclosure")) {
				int index;
				String url;
				currentitem.enclosure_url = attributes.getValue("url");
				url = currentitem.enclosure_url;
				if (url != null) {
					if (url.startsWith("http://"))
						url = url.substring(7);
					
					for (index = url.length() - 1; index >= 0; index--)
						if (url.charAt(index) == '/')
							break;
					
					if (index != -1) {
						int end_pos;
						String file_name;
						/* deal with URL ends which contains '?' parameter */
						if ((end_pos = url.indexOf("?")) > index + 1)
							file_name = currentfeed.title + "/" + url.substring(index + 1, end_pos);
						else
							file_name = currentfeed.title + "/" + url.substring(index + 1);

						String file_name_postfix = file_name;
						int point = file_name.indexOf(".");
						int postfix = 1;
						int i;
						for (i = 0; i < currentfeed.items.size(); i++) {
							if (file_name_postfix.equals(currentfeed.items.get(i).file_name)) {		
								if (point == -1)
									file_name_postfix = file_name + "(" + (postfix++) + ")";
								else
									file_name_postfix = file_name.substring(0, point) + "(" + (postfix++) + ")." + file_name.substring(point + 1);
							}
						}
						currentitem.file_name = file_name_postfix;
					}
				}
			}
		}
	}
	
	@Override
	public void endElement(String uri, String localName, String qName) throws SAXException {
		super.endElement(uri, localName, qName);
		
		/* </item> */
		if (localName.equalsIgnoreCase(TAG_ITEM)) {
			/* add RSSItem to RSSFeed */
			currentitem.parentFeed = currentfeed;
			currentfeed.items.add(currentitem);
			isParsing = false;
		}
		else if (currentindex != -1) {
			if (isParsing) {
				switch (currentindex) {
					case 0: currentitem.title = builder.toString();			break; 
					case 1:	currentitem.link = builder.toString();			break;
					case 2:
						try {
							Date date = readFormat.parse(builder.toString());
							currentitem.pubDate = date.getTime();
							currentitem.pubDate_str = printFormat.format(currentitem.pubDate);
						}
						catch (ParseException e) {
							currentitem.pubDate_str = "";
						}
						if (currentitem.pubDate_str.equals("")) {
							try {
								Date date = readFormat2.parse(builder.toString());
								currentitem.pubDate = date.getTime();
								currentitem.pubDate_str = printFormat.format(currentitem.pubDate);
							}
							catch (ParseException e) {
								currentitem.pubDate_str = "";
								e.printStackTrace();
							}
						}
						break;
					case 3:
						currentitem.description = builder.toString();
						int from, to;
						while (true) {
							from = currentitem.description.indexOf('<');
							to = currentitem.description.indexOf('>', from);
								if (from > 0 && to > 0)
									currentitem.description = currentitem.description.substring(0, from)
															  + currentitem.description.substring(to + 1);
								else break;
						}
						break;
				}
			}
			else {
				switch (currentindex) {
				case 0:
					if (currentfeed.title == null)
						currentfeed.title = builder.toString();
						File newDirectory = new File(ReadyCastFileIO.mSdPath + "/ReadyCast/" + currentfeed.title);
						newDirectory.mkdirs();
					break; 
				case 1:
					if (currentfeed.link == null)
						currentfeed.link = builder.toString();
					break;
				case 3:
					if (currentfeed.description == null)
						currentfeed.description = builder.toString();
					break;
				}
			}
		}
		
	}

	private int itemIndexFromString(String tagname) {
		int itemindex = -1;
		for (int index = 0; index < xmltags.length; index++)
		{
			if (tagname.equalsIgnoreCase(xmltags[index]))
			{
				itemindex = index;
				break;
			}
		}
		return itemindex;
	}
}
