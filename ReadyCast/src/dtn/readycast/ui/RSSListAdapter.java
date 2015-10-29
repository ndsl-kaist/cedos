package dtn.readycast.ui;

import java.util.ArrayList;
import java.util.List;

import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Filter;
import android.widget.Filterable;
import android.widget.ImageButton;
import android.widget.ProgressBar;
import android.annotation.SuppressLint;
import android.view.View;
import android.widget.TextView;
import dtn.readycast.R;
import dtn.readycast.ReadyCastFileIO;
import dtn.readycast.item.RSSItem;

@SuppressLint({ "DefaultLocale", "InflateParams" })
public class RSSListAdapter extends ArrayAdapter<RSSItem> implements Filterable {

	public ArrayList<RSSItem> objects = null;
	private ArrayList<RSSItem> mItemsArray = null;
	private ItemsFilter mFilter;
	private Tab1_Fragment main_fragment = null;
	private MainActivity main_activity;

	public RSSListAdapter(MainActivity context, int textviewid, ArrayList<RSSItem> objects) {
		super(context, textviewid, objects);
		main_fragment = (Tab1_Fragment)(context.current_fragment);
		this.objects = objects;
		main_activity = context;
	}

	@Override
	public int getCount() {
		return ((null != objects) ? objects.size() : 0);
	}

	@Override
	public long getItemId(int position) {
		return position;
	}

	@Override
	public RSSItem getItem(int position) {
		return ((null != objects) ? objects.get(position) : null);
	}

	@Override
	public Filter getFilter() {
		if (mFilter == null) {
			mFilter = new ItemsFilter();
		}
		return mFilter;
	}

	@Override
	public View getView(int position, View convertView, ViewGroup parent) {
		View view = convertView;

		if (null == view)
		{				
			view = main_fragment.vi.inflate(R.layout.xxx_itemview, null);
		}

		final RSSItem data = objects.get(position);

		if (null != data)
		{
			TextView title = (TextView)view.findViewById(R.item.txtTitle);
			TextView pubDate = (TextView)view.findViewById(R.item.txtPubDate);
			final ProgressBar prog = (ProgressBar)view.findViewById(R.item.progbar);
			TextView percent = (TextView)view.findViewById(R.item.txtPercent);

			title.setText(data.title);
			int len = data.pubDate_str.length() - 9;
			if (len < 0) len = 0;
			pubDate.setText(data.pubDate_str.substring(0, len));
			prog.setProgress(data.percent);
			if (data.status == RSSItem.NONE)
				percent.setText("");
			else
				percent.setText(data.percent + "%");

			/* default value for progress bar */
			prog.setIndeterminate(false);
			if (data.status == RSSItem.DOWNLOADING)					
				if (data.percent == 0 && data.enclosure_len == 0)
					prog.setIndeterminate(true);

			Button btnDownload = (Button)view.findViewById(R.item.btnDownload);
			Button btnPlay = (Button)view.findViewById(R.item.btnPlay);
			ImageButton btnCancel = (ImageButton)view.findViewById(R.item.btnCancel);
			ImageButton btnDiscard = (ImageButton)view.findViewById(R.item.btnDiscard);

			btnDownload.setOnClickListener(new OnClickListener() {
				@Override
				public void onClick(View v) {
					main_fragment.showDownloadWindow(data);
				}});
			btnPlay.setOnClickListener(new OnClickListener() {
				@Override
				public void onClick(View v) {
					main_fragment.playFile(data);
					if (data.status == RSSItem.DOWNLOADED) {
						data.status = RSSItem.DOWNLOADED_PLAYED;
						main_activity.updateRecent();
						ReadyCastFileIO.saveFeedToFile(data.parentFeed);
					}
				}});
			btnCancel.setOnClickListener(new OnClickListener() {
				@Override
				public void onClick(View v) {
					main_fragment.cancelDownload(data);
					main_fragment.deleteFile(data);
					prog.setIndeterminate(false);
				}});
			btnDiscard.setOnClickListener(new OnClickListener() {
				@Override
				public void onClick(View v) {
					main_fragment.deleteFile(data);
				}});

			if (data.status == RSSItem.NONE) {
				btnDownload.setVisibility(View.VISIBLE);
				btnPlay.setVisibility(View.GONE);
				btnCancel.setVisibility(View.GONE);
				btnDiscard.setVisibility(View.GONE);
			}
			else if (data.status == RSSItem.NOTFOUND) {
				prog.setVisibility(View.GONE);
				btnDownload.setVisibility(View.GONE);
				btnPlay.setVisibility(View.GONE);
			}
			else {
				btnDownload.setVisibility(View.GONE);
				btnPlay.setVisibility(View.VISIBLE);
				if (data.status == RSSItem.DOWNLOADING) {
					btnCancel.setVisibility(View.VISIBLE);
					btnDiscard.setVisibility(View.GONE);
				}
				else {
					btnCancel.setVisibility(View.GONE);
					btnDiscard.setVisibility(View.VISIBLE);
				}
			}			
		}

		return view;
	}

	/**********************************************************************************************/

	private class ItemsFilter extends Filter {
		private final Object mLock = new Object();

		@Override
		protected FilterResults performFiltering(CharSequence prefix) {
			// Initiate our results object
			FilterResults results = new FilterResults();
			// If the adapter array is empty, check the actual items array and use it
			if (mItemsArray == null) {
				synchronized (mLock) { // Notice the declaration above
					mItemsArray = new ArrayList<RSSItem>(objects);
				}
			}
			// No prefix is sent to filter by so we're going to send back the original array
			if (prefix == null || prefix.length() == 0) {
				synchronized (mLock) {
					results.values = mItemsArray;
					results.count = mItemsArray.size();
				}
			} else {
				// Compare lower case strings
				String prefixString = prefix.toString().toLowerCase();
				// Local to here so we're not changing actual array
				final List<RSSItem> items = mItemsArray;
				final int count = items.size();
				final List<RSSItem> newItems = new ArrayList<RSSItem>(count);
				for (int i = 0; i < count; i++) {
					final RSSItem item = items.get(i);
					final String itemName = item.title.toLowerCase();
					// First match against the whole, non-splitted value
					if (itemName.startsWith(prefixString)) {
						newItems.add(item);
					}
				}
				// Set and return
				results.values = newItems;
				results.count = newItems.size();
			}
			return results;
		}
		@Override
		@SuppressWarnings("unchecked")
		protected void publishResults(CharSequence prefix, FilterResults results) {
			//noinspection unchecked
			objects = (ArrayList<RSSItem>) results.values;
			// Let the adapter know about the updated list
			if (results.count > 0) {
				notifyDataSetChanged();
			} else {
				notifyDataSetInvalidated();
			}
		}
	}
	
	/**********************************************************************************************/


}