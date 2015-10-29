package dtn.readycast.ui;

import java.io.File;
import java.util.ArrayList;

import android.os.Environment;import android.os.RemoteException;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.BaseExpandableListAdapter;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ProgressBar;
import android.widget.TextView;

import android.view.View;
import dtn.readycast.R;
import dtn.readycast.ReadyCastFileIO;
import dtn.readycast.item.RSSFeed;
import dtn.readycast.item.RSSItem;

public class Tab2_ListAdapter extends BaseExpandableListAdapter {

	/* group 0: DOWNLOADED, group 1: DOWNLOADING */
	static final int GROUP_COUNT = 1;
	static final int DOWNLOADED_GROUP = 1;
	static final int DOWNLOADING_GROUP = 0;
	
	MainActivity main_activity;

	private LayoutInflater inflater;
	public ArrayList<RSSFeed> objects = null;

	public Tab2_ListAdapter(MainActivity m, ArrayList<RSSFeed> objects) {
		super();
		this.objects = objects;
		inflater = LayoutInflater.from(m);
		main_activity = m;
	}

	@Override
	// counts the number of group/parent items
	public int getGroupCount() {
		// TODO Auto-generated method stub
		return GROUP_COUNT;
	}

	@Override
	// counts the number of children items
	public int getChildrenCount(int i) {

		if (i == DOWNLOADING_GROUP)
			return main_activity.itemlist_downloading.size();
		else if (i == DOWNLOADED_GROUP)
			return main_activity.itemlist_downloaded.size();
		else
			return 0;
	}

	@Override
	// gets the title of each parent/group
	public Object getGroup(int i) {
		if (i == DOWNLOADING_GROUP)
			return "Downloading files ("
					+ Integer.toString(getChildrenCount(DOWNLOADING_GROUP))
					+ ")";
		else if (i == DOWNLOADED_GROUP)
			return "완료 ("
					+ Integer.toString(getChildrenCount(DOWNLOADED_GROUP))
					+ ") " + Integer.toString(main_activity.downloaded_filesize/1000000) + " MB";
		else
			return "Unexpected error";
	}

	private Object getChildObj(int i, int j) {
		Object ret = null;
		try {
		if (i == DOWNLOADING_GROUP)
			ret = main_activity.itemlist_downloading.get(j);
		else if (i == DOWNLOADED_GROUP)
			ret = main_activity.itemlist_downloaded.get(j);
		} catch (Exception e) {
			return null;
		}
		
		return ret;
	}

	@Override
	// gets the name of each item
	public Object getChild(int i, int j) {
		return ((RSSItem) getChildObj(i, j)).title;
	}

	@Override
	public long getGroupId(int groupPosition) {
		return groupPosition;
	}

	@Override
	public long getChildId(int groupPosition, int childPosition) {
		return childPosition;
	}

	@Override
	public boolean hasStableIds() {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean isChildSelectable(int groupPosition, int childPosition) {
		// TODO Auto-generated method stub
		return true;
	}

	/**********************************************************************************************/

	@Override
	public View getGroupView(int groupPosition, boolean isExpanded, View view,
			ViewGroup viewGroup) {

		ViewHolder holder = new ViewHolder();
		holder.groupPosition = groupPosition;

		if (view == null) {
			if (groupPosition == DOWNLOADED_GROUP)
				view = inflater.inflate(R.layout.tab2_item_parent_ed,
						viewGroup, false);
			else
				view = inflater.inflate(R.layout.tab2_item_parent, viewGroup,
						false);
		}

		TextView textView = (TextView) view
				.findViewById(R.id.list_item_text_view);
		// "i" is the position of the parent/group in the list
		textView.setText(getGroup(groupPosition).toString());

		if (groupPosition == DOWNLOADED_GROUP) {
			Button clearButton = (Button) view.findViewById(R.id.button /*btnDiscardAll*/);

			clearButton.setOnClickListener(new OnClickListener() {
				public void onClick(View v) {
					/* TODO: clear files */
					int iter;
					for (iter = 0; iter < main_activity.itemlist_downloaded.size(); iter++)
						deleteFile(main_activity.itemlist_downloaded.get(iter));
					main_activity.updateRecent();
						
				}
			});
		} else if (groupPosition == DOWNLOADING_GROUP) {
		}

		view.setTag(holder);

		// return the entire view
		return view;

	}
	
	public void cancelDownload (final RSSItem data) {
		if (main_activity.mService == null)
			return;
		
		try {			
			main_activity.mService.unregisterDownload(data.uuid);
		} catch (RemoteException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		data.status = RSSItem.NONE;
	}

	public void deleteFile (final RSSItem data) {
		File file = new File(Environment.getExternalStorageDirectory().getAbsolutePath() + "/ReadyCast/" + data.file_name);
		file.delete();
		data.percent = 0;
		data.enclosure_len = 0;

		data.status = RSSItem.NONE;
		ReadyCastFileIO.saveFeedToFile(data.parentFeed);
	}

	@Override
	public View getChildView(final int groupPosition, final int childPosition,
			boolean isLastChild, View view, ViewGroup viewGroup) {
		
		ViewHolder holder = new ViewHolder();
		holder.childPosition = childPosition;
		holder.groupPosition = groupPosition;

		if (view == null)
			view = inflater.inflate(R.layout.tab2_item_child_ed, viewGroup,
					false);
		
		ImageButton btnCancel;
		btnCancel = (ImageButton) view
				.findViewById(R.id.recent_btnCancel);
		final ProgressBar prog = (ProgressBar) view
				.findViewById(R.id.recent_progbar);
		
		btnCancel.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View v) {
				try {
					Log.d("app_debug", "button pressed");
				cancelDownload((RSSItem) getChildObj(groupPosition, childPosition));
				deleteFile((RSSItem) getChildObj(groupPosition, childPosition));
				prog.setIndeterminate(false);
				main_activity.updateRecent();
				notifyDataSetChanged();
				} catch (Exception e) {
					e.printStackTrace();
				}
			}});
		/*
		btnDiscard.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View v) {
				try {
				deleteFile((RSSItem) getChildObj(groupPosition, childPosition));
				main_activity.updateRecent();
				notifyDataSetChanged();
				} catch (Exception e) {
					e.printStackTrace();
				}
			}});
		*/
		try {
			TextView textView = (TextView) view
					.findViewById(R.id.list_item_text_child);
			// "i" is the position of the parent/group in the list and
			// "i1" is the position of the child
			textView.setText(((RSSItem) getChildObj(groupPosition,
					childPosition)).title);

			view.setTag(holder);

			if (groupPosition == DOWNLOADED_GROUP) {
				btnCancel.setVisibility(View.GONE);
				btnCancel.setEnabled(false);
				//btnDiscard.setVisibility(View.VISIBLE);
				prog.setVisibility(View.GONE);
				prog.setEnabled(false);
				btnCancel.invalidate();
				//btnDiscard.invalidate();
				prog.invalidate();
				
				TextView child_size = (TextView) view
						.findViewById(R.id.list_item_size_child);
				child_size.setText(Long
						.toString((((RSSItem) getChildObj(groupPosition,
								childPosition)).enclosure_len) / 1000000)
						+ " MB");
			}
			else if (groupPosition == DOWNLOADING_GROUP) {
				btnCancel.setVisibility(View.VISIBLE);
				//btnDiscard.setVisibility(View.GONE);
				//btnDiscard.setEnabled(false);
				prog.setVisibility(View.VISIBLE);
				btnCancel.invalidate();
				//btnDiscard.invalidate();
				prog.invalidate();
				
				long time_diff = ((RSSItem) getChildObj(
						groupPosition, childPosition)).deadline_sec
						- (System.currentTimeMillis() / 1000);
				String time_str =
						time_diff < 0 ? "Now" :
						time_diff < 60 ? Long.toString(time_diff) + "s" :
						time_diff < 3600 ? Long.toString(time_diff/60) + "m" :
						time_diff < 86400 ? Long.toString(time_diff/3600) + "h " + Long.toString((time_diff%3600)/60) + "m" :
						time_diff < 604800 ? Long.toString(time_diff/86400) + "d " + Long.toString((time_diff%86400)/3600) + "h" :
						Long.toString(time_diff/604800) + "w " + Long.toString((time_diff%604800)/86400) + "d";
				int perc = ((RSSItem) getChildObj(groupPosition,
						childPosition)).percent;
				if (perc == -2)
					prog.setIndeterminate(true);
				else {
					prog.setIndeterminate(false);
					prog.setProgress(perc);
				}
				TextView time_remain = (TextView) view
						.findViewById(R.id.list_item_size_child);
				time_remain.setText(time_str);
			}
		} catch (NullPointerException e) {
			e.printStackTrace();
			TextView textView = (TextView) view
					.findViewById(R.id.list_item_text_child);
			// "i" is the position of the parent/group in the list and
			// "i1" is the position of the child
			textView.setText("NullPointerException "
					+ Integer.toString(groupPosition) + " "
					+ Integer.toString(childPosition));
		}

		// return the entire view
		return view;
	}

	/**********************************************************************************************/
	protected class ViewHolder {
		protected int childPosition;
		protected int groupPosition;
	}
}