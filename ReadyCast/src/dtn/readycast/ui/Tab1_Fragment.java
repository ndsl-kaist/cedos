package dtn.readycast.ui;

import java.io.File;
import java.net.FileNameMap;
import java.net.URLConnection;
import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;

import dtn.readycast.R;
import dtn.readycast.ReadyCastFileIO;
import dtn.readycast.item.RSSFeed;
import dtn.readycast.item.RSSItem;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.os.RemoteException;
import android.support.v4.app.ListFragment;
import android.text.method.ScrollingMovementMethod;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.View.OnClickListener;
import android.view.animation.AnimationUtils;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemLongClickListener;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ViewSwitcher;

public class Tab1_Fragment extends ListFragment {
	public Tab1_ListAdapter rssadapter = null;
	public RSSListAdapter rssadapter2 = null;
	public String playingPath;
	public boolean pendingUIRefreshEvent = true;

	public LayoutInflater vi;
	public static String info = null;

	public static Handler handle;
	public static final int STAT_UPDATED       = 1;
	public static final int STAT_UPDATE_NOW    = 3;

	private Tab1_SubDialog dialog;
	private ViewSwitcher vs;

	/* onAttach the fragment */
	public MainActivity main_activity;

	private int depth = 0;
	
	private Timer timer = null;
	
	public Spinner s;
	public ArrayAdapter<String> spinadapter = null;
	
	@Override
	public void onCreate(Bundle savedInstanceState) 
	{
		super.onCreate(savedInstanceState);
		depth = 0;
		
		createHandler();

		if (timer == null) {
			timer = new Timer(); 
			timer.scheduleAtFixedRate(new TimerTask() 
			    { 
			        public void run() 
			        { 
			        	if (pendingUIRefreshEvent) {
							Message a = Tab1_Fragment.handle.obtainMessage();
							a.what = Tab1_Fragment.STAT_UPDATE_NOW;
							Tab1_Fragment.handle.sendMessage(a);
			        	}
			        	pendingUIRefreshEvent = false;
			        } 
			    }, 0, 500); 
		}
	}

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);
		main_activity = (MainActivity) activity;
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {

		// TODO Auto-generated method stub
		View myFragmentView = (View) inflater.inflate(R.layout.tab1_view, container, false);
		vi = inflater;

		vs = (ViewSwitcher) myFragmentView.findViewById(R.channel.viewswitcher);

		rssadapter = new Tab1_ListAdapter(main_activity, R.layout.tab1_item, main_activity.feedlist);
		setListAdapter(rssadapter);

		return super.onCreateView(inflater, container, savedInstanceState);
	}


            
	@Override
	public void onActivityCreated(Bundle savedState) {
		super.onActivityCreated(savedState);

		getListView().setOnItemLongClickListener(new OnItemLongClickListener() {

			@Override
			public boolean onItemLongClick(AdapterView<?> arg0, View arg1,
					final int position, long id) {
				
				final RSSFeed selected = main_activity.feedlist.get(position);
				final String[] options = getResources().getStringArray(R.array.options);
				AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
				builder.setTitle(selected.title);
				
				
				builder.setItems(options, new DialogInterface.OnClickListener() {
					@Override
					public void onClick(DialogInterface dialog, int which) {
						// TODO Auto-generated method stub
						switch (which) {						
						case 0:
							Dialog dialog2 = new Dialog(main_activity, android.R.style.Theme_Holo_Light_DialogWhenLarge);
							dialog2.setContentView(R.layout.tab3_dialog);
							dialog2.setTitle("Podcast");
							//dialog2.setTitle("새 팟캐스트 구독");
							new Tab3_SubDialog(selected.title, dialog2, null, 1).execute(selected.itunes_id);
							dialog2.show();
							break;
						case 1:
							// remove the feed from file 
							ReadyCastFileIO.removeFeedFromFile(main_activity.feedlist.get(position));
							// remove current feed from feedlist
							main_activity.feedlist.remove(position);
							// save changed feedlist information into file
							ReadyCastFileIO.saveFeedListToFile(main_activity.feedlist);
							rssadapter.notifyDataSetChanged();
							break;
						}
					}
				});
				builder.create().show();
				
				return true;
			}});
	}

	@Override
	public void onListItemClick(ListView l, View v, int position, long id) {
		if (depth == 0) {
			/*
			depth++;
			vs.setInAnimation(AnimationUtils.loadAnimation(main_activity, R.anim.slide_in_right));
			vs.setOutAnimation(AnimationUtils.loadAnimation(main_activity, R.anim.slide_out_left));
			rssadapter2 = new RSSListAdapter(main_activity, R.layout.scheduleview, main_activity.feedlist.get(position).items);
			setListAdapter(rssadapter2);
			*/
			
			dialog = new Tab1_SubDialog(main_activity, android.R.style.Theme_Holo_Light_Dialog_MinWidth, main_activity.feedlist.get(position));
			dialog.show();
		}
		else {
		}

		super.onListItemClick(l, v, position, id);
	}
	
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if (keyCode == KeyEvent.KEYCODE_BACK) {
			if (depth > 0) {
				vs.setInAnimation(AnimationUtils.loadAnimation(main_activity, R.anim.slide_in_left));
				vs.setOutAnimation(AnimationUtils.loadAnimation(main_activity, R.anim.slide_out_right));
				vs.showPrevious();
				
				rssadapter = new Tab1_ListAdapter(main_activity, R.layout.tab1_item, main_activity.feedlist);
				setListAdapter(rssadapter);
				depth--;
				return true;
			}
			else
				return false;
		}
		return false;
	}
	
	
	
/*--------------------------------------------------------------------------------------------------*/
	// sub-methods called by ChannelListFragment

	public void showDownloadWindow (final RSSItem data) {
		final Dialog dialog = new Dialog (main_activity);
		dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
		dialog.setContentView( R.layout.xxx_detailview );
		WindowManager.LayoutParams params = dialog.getWindow().getAttributes();
		params.width = WindowManager.LayoutParams.MATCH_PARENT;
		
		if (main_activity.mService == null) {
			Intent intent = new Intent("dtn.net.service.DTPDownloadService");
			if ((main_activity.bindService(intent, main_activity.mConnection, Context.BIND_AUTO_CREATE)) == true);
		}

		TextView t1 = (TextView) dialog.findViewById(R.detail.txtTitle);
		TextView t2 = (TextView) dialog.findViewById(R.detail.txtDate);
		TextView t3 = (TextView) dialog.findViewById(R.detail.txtDescript);

		t1.setText(data.title);
		t2.setText("on " + data.pubDate_str);
		t3.setText(data.description);
		t3.setMovementMethod(ScrollingMovementMethod.getInstance());

		dialog.show();

		final Button b1 = (Button) dialog.findViewById(R.detail.btnDownload);
		b1.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View v) {
				if (data.enclosure_url == null) {
					Toast.makeText(main_activity, "No file to download", Toast.LENGTH_SHORT).show();
					return;
				}

				data.status = RSSItem.DOWNLOADING;
				main_activity.updateRecent();
				uiRefresh();

				b1.setEnabled(false);
				b1.setText("Downloading");

				data.initCallback();
				try {
					int deadlineSec = 0;
					final EditText t = (EditText) dialog.findViewById(R.detail.txtDeadline);
					if (!t.getText().toString().equals(""))
						deadlineSec = Integer.parseInt(t.getText().toString()) * 60;

					data.uuid = main_activity.mService.registerDownload(data.mCallback, data.enclosure_url, 
								Environment.getExternalStorageDirectory().getAbsolutePath() + "/ReadyCast/" + data.file_name,
								deadlineSec);
				} catch (RemoteException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}

				ReadyCastFileIO.saveFeedToFile(main_activity.feedlist.get(main_activity.current_feed));
				
				dialog.dismiss();
			}
		});

		Button b3 = (Button) dialog.findViewById(R.detail.btnCancel);
		b3.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View v) {
				dialog.dismiss();
			}
		});
	}

	public void cancelDownload (final RSSItem data) {
		if (main_activity.mService == null)
			return;
		
		try {			
			main_activity.mService.unregisterDownload(data.enclosure_url);
		} catch (RemoteException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		data.status = RSSItem.NONE;
		main_activity.updateRecent();
	}

	public void deleteFile (final RSSItem data) {
		File file = new File(Environment.getExternalStorageDirectory().getAbsolutePath() + "/ReadyCast/" + data.file_name);
		file.delete();
		data.percent = 0;
		data.enclosure_len = 0;
		uiRefresh();

		data.status = RSSItem.NONE;
		main_activity.updateRecent();
		ReadyCastFileIO.saveFeedToFile(data.parentFeed);
	}

	public void playFile (final RSSItem data) {

		try {
			if (data.file_type == null) {
				FileNameMap fileNameMap = URLConnection.getFileNameMap();
				data.file_type = fileNameMap.getContentTypeFor(ReadyCastFileIO.mSdPath + "/ReadyCast/" + data.file_name);
				if (data.file_type.contains("video"))
					data.file_type = "video/*";
			}

			if (data.file_type != null) {
				Intent intent = new Intent(Intent.ACTION_VIEW);
				Uri uri = Uri.fromFile(new File(ReadyCastFileIO.mSdPath + "/ReadyCast/" + data.file_name));
				intent.setDataAndType(uri, data.file_type);
				startActivity(intent);
				Toast.makeText(main_activity, "Now Playing...\n" + data.title, Toast.LENGTH_SHORT).show();
			}
			else
				Toast.makeText(main_activity, "Unknown file type\n" + data.title, Toast.LENGTH_SHORT).show();

		} catch (IllegalArgumentException e) {
			Toast.makeText(main_activity, "IllegalArgumentException", Toast.LENGTH_SHORT).show();
			e.printStackTrace();
		} catch (IllegalStateException e) {
			Toast.makeText(main_activity, "IllegalStateException", Toast.LENGTH_SHORT).show();
			e.printStackTrace();
		} catch (ActivityNotFoundException e) {
			Toast.makeText(main_activity, "Not playable in this device, please install another player " +
					"which can support " + data.file_type + " format.", Toast.LENGTH_SHORT).show();
			e.printStackTrace();
		}
	}

	public void loadSpinner(View v) {
		ArrayList<String> spinArray = new ArrayList<String>();
		for (int i = 0; i < main_activity.feedlist.size(); i++)
			spinArray.add(main_activity.feedlist.get(i).title);

		s = (Spinner) v.findViewById(R.main.spnFeeds);
		spinadapter = new ArrayAdapter <String>(main_activity, android.R.layout.simple_spinner_item, spinArray);
		spinadapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		s.setAdapter(spinadapter);
		s.setOnItemSelectedListener(new OnItemSelectedListener() {
			@Override
			public void onItemSelected(AdapterView<?> parentView, View selectedItemView, int position, long id) {
				RSSFeed f = main_activity.feedlist.get(position);
				if (f!= null && rssadapter != null) {
					main_activity.current_feed = position;
					rssadapter2.objects = f.items;
					uiRefresh();
					getListView().setSelectionAfterHeaderView();
				}
			}
			@Override
			public void onNothingSelected(AdapterView<?> parentView) {}
		});
	}

	public void uiRefresh() {
		pendingUIRefreshEvent = true;
	}
	

	@SuppressLint("HandlerLeak")
	public void createHandler() {
		/* defines handler for communication between threads */
		handle = new Handler() {
			@Override
			public void handleMessage(Message msg) {
				super.handleMessage(msg);

				switch (msg.what) {
				
				case STAT_UPDATE_NOW:
					if (rssadapter2 != null) {
		    			rssadapter2.notifyDataSetChanged();
		    		}
					break;
					
				case STAT_UPDATED:
					uiRefresh();

	    			if (dialog != null && dialog.isShowing())
	    				dialog.refreshALL();
					break;

				}

			}
		};

	}




}
