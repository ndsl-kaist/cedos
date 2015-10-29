package dtn.readycast.ui;

import java.util.ArrayList;
import java.util.Locale;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.Filter;
import android.widget.Filterable;
import android.widget.TextView;
import dtn.readycast.R;

public class FilterAdapter extends BaseAdapter implements Filterable {
	private LayoutInflater inflater = null;
	ArrayAdapter<String> full_array;

	ArrayList<String> full_title;
	ArrayList<String> filtered_title;

	public FilterAdapter(ArrayList<String> titles) {
		full_title = titles;
		filtered_title = titles;
	}

	public ArrayList<String> getFilteredList() {
		return filtered_title;
	}

	@Override
	public Filter getFilter() {
		return new Filter() {
			@Override
			protected FilterResults performFiltering(CharSequence constraint) {

				FilterResults results = new FilterResults();
				if (constraint == null || constraint.length() == 0) {
					results.values = full_title;
					results.count = full_title.size();
				} else {
					ArrayList<String> filtered = new ArrayList<String>();

					for (String p : full_title) {
						if (p.toUpperCase(Locale.US).startsWith(
								constraint.toString().toUpperCase(Locale.US))) {
							filtered.add(p);
						}
					}
					results.values = filtered;
					results.count = filtered.size();
				}
				return results;
			}

			@SuppressWarnings("unchecked")
			@Override
			protected void publishResults(CharSequence constraint,
					FilterResults results) {
				if (results.count == 0) {
					notifyDataSetInvalidated();
				} else {
					filtered_title = (ArrayList<String>) results.values;
					notifyDataSetChanged();
				}
			}
		};
	}

	@Override
	public int getCount() {
		return full_title.size();
	}

	@Override
	public Object getItem(int position) {
		return full_title.get(position);
	}

	@Override
	public long getItemId(int position) {
		return position;
	}

	@Override
	public View getView(int position, View convertView, ViewGroup parent) {
		if (convertView == null) {
			inflater = LayoutInflater.from(parent.getContext());
			convertView = inflater.inflate(R.layout.tab3_view_item, parent,
					false);
		}

		TextView title = (TextView) convertView.findViewById(R.id.title_data);
		if (position < filtered_title.size()) {
			title.setText("" + filtered_title.get(position));
		}

		return convertView;
	}
}
