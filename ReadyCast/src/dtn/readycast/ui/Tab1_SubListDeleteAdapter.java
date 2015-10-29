package dtn.readycast.ui;

import java.util.ArrayList;
import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Typeface;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.CheckedTextView;
import dtn.readycast.R;
import dtn.readycast.item.RSSItem;

public class Tab1_SubListDeleteAdapter extends ArrayAdapter<RSSItem> {
	public ArrayList<RSSItem> objects = null;
	private LayoutInflater inflater = null;

	public Tab1_SubListDeleteAdapter(Context context, int textviewid,
			ArrayList<RSSItem> objects) {
		super(context, textviewid, objects);
		this.objects = objects;
		this.inflater = LayoutInflater.from(context);
	}

	@SuppressLint("InflateParams")
	@Override
	public View getView(int position, View convertView, ViewGroup parent) {
		View view = convertView;

		if (null == view) {
			view = inflater.inflate(R.layout.tab1_dialog_item2, null);
		}

		RSSItem data = objects.get(position);
		if (data != null) {
			CheckedTextView title = (CheckedTextView) view
					.findViewById(R.id.text1);
			title.setText(data.title);

			if (data.status != RSSItem.DOWNLOADED_PLAYED)
				title.setTypeface(null, Typeface.BOLD);
			else
				title.setTypeface(null, Typeface.NORMAL);
		}

		return view;
	}
}
