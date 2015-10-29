package dtn.readycast.ui;

import java.util.ArrayList;

import android.annotation.SuppressLint;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Environment;
import android.util.Log;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Filterable;
import android.widget.ImageView;

import android.view.View;
import android.widget.TextView;
import dtn.readycast.R;
import dtn.readycast.item.RSSFeed;

@SuppressLint("DefaultLocale")
public class Tab1_ListAdapter extends ArrayAdapter<RSSFeed> implements Filterable {

	public ArrayList<RSSFeed> objects = null;
	private Tab1_Fragment main_fragment = null;

	public Tab1_ListAdapter(MainActivity context, int textviewid, ArrayList<RSSFeed> objects) {
		super(context, textviewid, objects);
		main_fragment = (Tab1_Fragment)(context.current_fragment);
		this.objects = objects;
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
	public RSSFeed getItem(int position) {
		return ((null != objects) ? objects.get(position) : null);
	}

	@Override
	public View getView(int position, View convertView, ViewGroup parent) {
		View view = convertView;

		if (null == view)
		{
			view = main_fragment.vi.inflate(R.layout.tab1_item, null);
		}

		final RSSFeed data = objects.get(position);

		if (null != data)
		{
			TextView title = (TextView)view.findViewById(R.sched.txtTitle);
			title.setText(data.title);
			
			int count = data.getNumNewDownloaded();
			TextView count_new = (TextView)view.findViewById(R.sched.txtCount2);
			if (count == 0)
				count_new.setVisibility(View.GONE);
			else {
				count_new.setVisibility(View.VISIBLE);
				count_new.setText(" " + count + " ");
			}

			TextView status = (TextView)view.findViewById(R.sched.txtDown);
			ImageView img = (ImageView)view.findViewById(R.sched.imgDown);
			if (data.getNumDownloading() > 0) {
				status.setText(data.getNumDownloadingText(getContext(), true));
				img.setVisibility(View.VISIBLE);
			}
			else {
				status.setText("");
				img.setVisibility(View.GONE);
			}
			
			TextView sched = (TextView)view.findViewById(R.sched.txtSched);
			sched.setText(data.getScheduleText(getContext()));
				
			if (data.itunes_id != null) {
				ImageView imageview = (ImageView) view.findViewById(R.sched.imageView);
				Bitmap myBitmap = BitmapFactory.decodeFile(Environment.getExternalStorageDirectory().getPath() + "/ReadyCast/thumb_image/" + data.itunes_id + ".jpg");
				imageview.setImageBitmap(myBitmap);
			}
			else {
				Log.d("appdtp", "NOPE");
			}
		}

		return view;
	}
	
	/**********************************************************************************************/

}