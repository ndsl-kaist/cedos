package dtn.readycast.ui;

import java.util.Timer;
import java.util.TimerTask;

import dtn.readycast.R;
import android.annotation.SuppressLint;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ExpandableListView;
import dtn.readycast.ui.MainActivity;

public class Tab2_Fragment extends Fragment {
	public static LayoutInflater vi;
	public ExpandableListView lv;
	public Tab2_ListAdapter adapter;
	public static Handler handle;
	private Timer timer = null;
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		createHandler();
		Log.d("app_debug", "onCreate");
		if (timer == null) {
			timer = new Timer();
			timer.scheduleAtFixedRate(new TimerTask() {
				public void run() {
					Message a = Tab2_Fragment.handle.obtainMessage();
					Tab2_Fragment.handle.sendMessage(a);
				}
			}, 0, 1000);
		}
	}

	@Override
	public void onDestroy() {
		super.onDestroy();
		Log.d("app_debug", "onDestroy");
		timer.cancel();
		timer.purge();
	}
	
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {

		View myFragmentView = (View) inflater.inflate(R.layout.tab2_view, container, false);
		vi = inflater;
		
		lv = (ExpandableListView) myFragmentView.findViewById(R.id.recentlist);

		/* create ExpandableListView's adapter */		
		adapter = new Tab2_ListAdapter((MainActivity)getActivity(), ((MainActivity)getActivity()).feedlist);
		lv.setAdapter(adapter);
		
		lv.setOnGroupClickListener(new ExpandableListView.OnGroupClickListener() {
			  @Override
			  public boolean onGroupClick(ExpandableListView parent, View v, int groupPosition, long id) {
			      // Doing nothing
			    return true;
			  }
			});
		
		lv.expandGroup(Tab2_ListAdapter.DOWNLOADING_GROUP);
		
		return myFragmentView;
	}
	

	@SuppressLint("HandlerLeak")
	public void createHandler() {
		/* defines handler for communication between threads */
		handle = new Handler() {
			@Override
			public void handleMessage(Message msg) {
				Log.d("app_debug", "handleMessage");
				super.handleMessage(msg);
					if (adapter != null)
		    			adapter.notifyDataSetChanged();
			}
		};
	}
}
