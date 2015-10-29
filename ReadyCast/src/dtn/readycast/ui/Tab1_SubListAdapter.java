package dtn.readycast.ui;

import java.util.ArrayList;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Typeface;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;

import android.view.View;
import android.widget.TextView;
import dtn.readycast.R;
import dtn.readycast.item.RSSItem;

@SuppressLint("DefaultLocale")
public class Tab1_SubListAdapter extends ArrayAdapter<RSSItem> {

	public ArrayList<RSSItem> objects = null;
    private LayoutInflater inflater = null;

	public Tab1_SubListAdapter(Context context, int textviewid, ArrayList<RSSItem> objects) {
		super(context, textviewid, objects);
		this.objects = objects;
		this.inflater = LayoutInflater.from(context);
	}

	@SuppressLint("InflateParams")
	@Override
	public View getView(int position, View convertView, ViewGroup parent) {
		View view = convertView;

		if (null == view)
		{
			view = inflater.inflate(R.layout.tab1_dialog_item1, null);
		}
		
		RSSItem data = objects.get(position);
		
		if (data != null) {		
			TextView title = (TextView)view.findViewById(R.tdi1.txtTitle);
			title.setText(data.title);
	
			if (data.status != RSSItem.DOWNLOADED_PLAYED)
				title.setTypeface(null, Typeface.BOLD);
			else
				title.setTypeface(null, Typeface.NORMAL);
		}
		
		return view;
	}
	
	/**********************************************************************************************/

}