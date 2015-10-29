package dtn.readycast.ui;

import java.io.File;
import java.net.FileNameMap;
import java.net.URLConnection;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

import android.annotation.SuppressLint;
import android.app.ActionBar;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.FragmentTransaction;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.PowerManager;
import android.os.RemoteException;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.Window;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import dtn.net.service.DTPDownloadInterface;
import dtn.readycast.R;
import dtn.readycast.ReadyCastFileIO;
import dtn.readycast.item.RSSFeed;
import dtn.readycast.item.RSSItem;
import dtn.readycast.item.RSSRetrieveFeeds;

@SuppressWarnings("deprecation")
public class MainActivity extends FragmentActivity implements
		ActionBar.TabListener {
	public Fragment current_fragment;

	public static Handler main_handle;

	public static final int LIST_UPDATED = 1;
	public static final int CONN_CLOSED = 2;
	public static final int SYSTEM_SHUTDOWN = 3;

	public NotificationManager notifier = null;
	public ArrayList<RSSFeed> feedlist = new ArrayList<RSSFeed>();
	public int current_feed;
	public int default_deadline = 0;
	Intent intent_scheduling;

	SharedPreferences prefs;
	Button btnClosePopup;

	/* Additional list to track downloading & downloaded items */
	public ArrayList<RSSItem> itemlist_downloading = new ArrayList<RSSItem>();
	public ArrayList<RSSItem> itemlist_downloaded = new ArrayList<RSSItem>();
	public int downloaded_filesize = 0;

	TelephonyManager mngr;

	// 140527
	PowerManager powerMgr;
    PowerManager.WakeLock m_wlWakeLock = null;
    
	/**
	 * The serialization (saved instance state) Bundle key representing the
	 * current tab position.
	 */
	private static final String STATE_SELECTED_NAVIGATION_ITEM = "selected_navigation_item";

	/* service connection definition */
	public DTPDownloadInterface mService = null;
	// Service Connection
	protected ServiceConnection mConnection = new ServiceConnection() {
		public void onServiceConnected(ComponentName className, IBinder service) {
			mService = DTPDownloadInterface.Stub.asInterface(service);
			Log.i("appdtp", "onServiceConnected");
			if (mService != null)
				Log.i("appdtp", "mService is not null");
		}

		public void onServiceDisconnected(ComponentName className) {
			mService = null;
		}
	};

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main_view);
		
		// 140527
	    powerMgr = (PowerManager)this.getSystemService(Context.POWER_SERVICE);
		m_wlWakeLock = powerMgr.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "Downloader");
		m_wlWakeLock.acquire();
		
		/* retrieve RSS feeds */
		new RSSRetrieveFeeds(this, -1).execute("");
		prefs = PreferenceManager.getDefaultSharedPreferences(this);

		// If the service is being started or is already running, the
		// ComponentName of the actual service that was started is returned;
		// else if the service does not exist null is returned.
		Intent intent = new Intent("dtn.net.service.DTPDownloadService");
		if ((getApplicationContext().bindService(intent, mConnection,
				Context.BIND_AUTO_CREATE)) == true)
			;

		// Set up the action bar to show tabs.
		final ActionBar actionBar = getActionBar();
		actionBar.setNavigationMode(ActionBar.NAVIGATION_MODE_TABS);

		// For each of the sections in the app, add a tab to the action bar.

		actionBar.addTab(actionBar.newTab().setText(R.string.main_tab1_title)
				.setTabListener(this));
		actionBar.addTab(actionBar.newTab().setText(R.string.main_tab2_title)
				.setTabListener(this));
		actionBar.addTab(actionBar.newTab().setText(R.string.main_tab3_title)
				.setTabListener(this));

		actionBar.setDisplayShowTitleEnabled(false);
		actionBar.setDisplayShowHomeEnabled(false);

		actionBar.setDisplayShowTitleEnabled(true);
		actionBar.setDisplayShowHomeEnabled(true);

		if (notifier == null)
			notifier = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);


		mngr = (TelephonyManager) (getSystemService(Context.TELEPHONY_SERVICE));

		// get default deadline value from settings
		default_deadline = Integer.parseInt(prefs.getString("prefDeadline",
				Integer.toString(default_deadline)));
		
	}

	// check if the required service is running
	public boolean isServiceExists(String serviceName) {
		ActivityManager am = (ActivityManager) this
				.getSystemService(ACTIVITY_SERVICE);
		List<ActivityManager.RunningServiceInfo> rs = am.getRunningServices(50);
		boolean serviceAlive = false;
		for (int i = 0; i < rs.size(); i++) {
			ActivityManager.RunningServiceInfo rsi = rs.get(i);
			if (rsi.service.getClassName().equals(serviceName)) {
				serviceAlive = true;
			}
		}
		return serviceAlive;
	}

	@SuppressLint("Wakelock")
	@Override
	public void onDestroy() {

		Log.d("readycast", "MainActivity: onDestroy()");

		m_wlWakeLock.release();
		/* cancel all notifications on exit */
		if (notifier != null)
			notifier.cancelAll();

		getApplicationContext().unbindService(mConnection);

		super.onDestroy();
	}

	public void updateRecent() {
		Comparator<RSSItem> myComparator = null;
		itemlist_downloading.clear();
		itemlist_downloaded.clear();
		downloaded_filesize = 0;

		int iter1, iter2;
		for (iter1 = 0; iter1 < feedlist.size(); iter1++)
			for (iter2 = 0; iter2 < feedlist.get(iter1).items.size(); iter2++) {
				if (feedlist.get(iter1).items.get(iter2).status == RSSItem.DOWNLOADING)
					itemlist_downloading.add(feedlist.get(iter1).items
							.get(iter2));
				if (feedlist.get(iter1).items.get(iter2).status == RSSItem.DOWNLOADED) {
					itemlist_downloaded.add(feedlist.get(iter1).items
							.get(iter2));
					downloaded_filesize += feedlist.get(iter1).items.get(iter2).enclosure_len;
				}
			}

		myComparator = new Comparator<RSSItem>() {
			@Override
			public int compare(RSSItem object1, RSSItem object2) {
				return object1.deadline_sec < object2.deadline_sec ? -1
						: object1.deadline_sec > object2.deadline_sec ? 1 : 0;
			}
		};
		Collections.sort(itemlist_downloading, myComparator);

		myComparator = new Comparator<RSSItem>() {
			@Override
			public int compare(RSSItem object1, RSSItem object2) {
				return object1.enclosure_len > object2.enclosure_len ? -1
						: object1.enclosure_len < object2.enclosure_len ? 1 : 0;
			}
		};
		Collections.sort(itemlist_downloaded, myComparator);

	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {

		switch (getActionBar().getSelectedNavigationIndex()) {
		case 0:
			/* onKeyDown event in first tab */
			Tab1_Fragment a = (Tab1_Fragment) current_fragment;
			if (a.onKeyDown(keyCode, event))
				return true;
			else
				break;
		case 1:
			/* onKeyDown event in second tab */
			/*
			 * RecentListFragment b = (RecentListFragment) current_fragment; if
			 * (b.onKeyDown(keyCode, event)) return true; else break;
			 */
			break;
		case 2:
			Tab3_Fragment c = (Tab3_Fragment) current_fragment;
			if (c.onKeyDown(keyCode, event))
				return true;
			else
				break;
		}
		// and so on...
		return super.onKeyDown(keyCode, event);
	}

	@Override
	public void onRestoreInstanceState(Bundle savedInstanceState) {
		// Restore the previously serialized current tab position.
		if (savedInstanceState.containsKey(STATE_SELECTED_NAVIGATION_ITEM)) {
			getActionBar().setSelectedNavigationItem(
					savedInstanceState.getInt(STATE_SELECTED_NAVIGATION_ITEM));
		}
	}

	@Override
	public void onSaveInstanceState(Bundle outState) {
		// Serialize the current tab position.
		outState.putInt(STATE_SELECTED_NAVIGATION_ITEM, getActionBar()
				.getSelectedNavigationIndex());
	}

	@Override
	public void onTabSelected(ActionBar.Tab tab,
			FragmentTransaction fragmentTransaction) {

		// When the given tab is selected, show the tab contents in the
		// container view.
		switch (tab.getPosition()) {
		case 0:
			current_fragment = new Tab1_Fragment();
			break;
		case 1:
			current_fragment = new Tab2_Fragment();
			break;
		case 2:
			current_fragment = new Tab3_Fragment();
			break;
		}
		getSupportFragmentManager().beginTransaction()
				.replace(R.id.container, current_fragment).commit();

	}

	@Override
	public void onTabUnselected(ActionBar.Tab tab,
			FragmentTransaction fragmentTransaction) {
	}

	@Override
	public void onTabReselected(ActionBar.Tab tab,
			FragmentTransaction fragmentTransaction) {
	}

	public void refreshCurrentTab() {
		try {
			current_fragment = new Tab1_Fragment();
	
			getSupportFragmentManager().beginTransaction()
					.replace(R.id.container, current_fragment).commit();
	
			getActionBar().setSelectedNavigationItem(0);
		}
		catch (IllegalStateException e) {
			e.printStackTrace();
		}
	}

	public void refreshCurrentTab2() {
		try {
			current_fragment = new Tab2_Fragment();
	
			getSupportFragmentManager().beginTransaction()
					.replace(R.id.container, current_fragment).commit();
	
			getActionBar().setSelectedNavigationItem(1);
		}
		catch (IllegalStateException e) {
			e.printStackTrace();
		}
	}

	/***************************************************************************/
	// added from RSSListActivity

	@Override
	public void onNewIntent(Intent intent) {
		super.onNewIntent(intent);

		Uri url = intent.getData();
		if (url != null)
			new RSSRetrieveFeeds(this, -1).execute(url.toString());

		if (intent.getAction() == AUDIO_SERVICE) {
			Intent play_intent = new Intent(Intent.ACTION_VIEW);
			Uri uri = Uri.fromFile(new File(ReadyCastFileIO.mSdPath
					+ "/ReadyCast/" + intent.getStringExtra("path")));
			play_intent.setDataAndType(uri, intent.getStringExtra("type"));
			startActivity(play_intent);
			Toast.makeText(this,
					"Now Playing...\n" + intent.getStringExtra("title"),
					Toast.LENGTH_SHORT).show();
		}
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		int menuGroupid = 0;
		int menuOrder = Menu.NONE;

		
		 menu.add(menuGroupid, Menu.FIRST, menuOrder,
		 "Add New Feeds").setIcon( android.R.drawable.ic_input_add);
		 /*
		 * menu.add(menuGroupid, Menu.FIRST + 1, menuOrder, "Show statistics")
		 * .setIcon(android.R.drawable.ic_menu_info_details);
		 * menu.add(menuGroupid, Menu.FIRST + 2, menuOrder, "Exit").setIcon(
		 * android.R.drawable.ic_lock_power_off);
		 */
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.main_activity_actions, menu);

		return super.onCreateOptionsMenu(menu);
	}

	/* Handles item selections */
	public boolean onOptionsItemSelected(MenuItem item) {
		AlertDialog alert;
		AlertDialog.Builder alt_bld;
		final Dialog dialog;
		switch (item.getItemId()) {

		/* Add a button on actionbar */
		case R.id.action_settings:
			startActivityForResult(new Intent(this, Settings.class),
					RESULT_FIRST_USER);
			break;

		/* Add a New Feed */
		case Menu.FIRST:
			dialog = new Dialog(this);
			dialog.setTitle("Add new feeds");
			dialog.requestWindowFeature(Window.FEATURE_LEFT_ICON);
			dialog.setContentView(R.layout.etc_dialog_newfeed);
			dialog.setFeatureDrawableResource(Window.FEATURE_LEFT_ICON,
					R.drawable.feed_icon);

			TextView d1 = (TextView) dialog.findViewById(R.addfeed.linkSBS);
			d1.setOnClickListener(new OnClickListener() {
				@Override
				public void onClick(View v) {
					dialog.dismiss();
				}
			});
			dialog.show();

			final Button bt1 = (Button) dialog.findViewById(R.addfeed.btnAdd);
			bt1.setOnClickListener(new OnClickListener() {
				@Override
				public void onClick(View v) {
					final EditText txt1 = (EditText) dialog
							.findViewById(R.addfeed.txtAddr);
					new RSSRetrieveFeeds(MainActivity.this, -1).execute(txt1
							.getText().toString());
					dialog.dismiss();
				}
			});
			Button bt2 = (Button) dialog.findViewById(R.addfeed.btnCancel);
			bt2.setOnClickListener(new OnClickListener() {
				@Override
				public void onClick(View v) {
					dialog.dismiss();
				}
			});
			return true;

			/* Statistics */
		case Menu.FIRST + 1:
			dialog = new Dialog(this);
			dialog.setTitle("Statistics");
			dialog.requestWindowFeature(Window.FEATURE_LEFT_ICON);
			dialog.setContentView(R.layout.etc_dialog_stat);
			dialog.setFeatureDrawableResource(Window.FEATURE_LEFT_ICON,
					R.drawable.feed_icon);

			dialog.show();
			Button bt3 = (Button) dialog.findViewById(R.stat.btnOK);
			bt3.setOnClickListener(new OnClickListener() {
				@Override
				public void onClick(View v) {
					dialog.dismiss();
				}
			});
			return true;

			/* Exit */
		case Menu.FIRST + 2:
			alt_bld = new AlertDialog.Builder(this);
			alt_bld.setMessage("Exit program?")
					.setPositiveButton("Yes",
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int id) {
									finish();
								}
							})
					.setNegativeButton("No",
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int id) {
									dialog.cancel();
								}
							});
			alert = alt_bld.create();
			alert.setTitle("ReadyCast");
			alert.setIcon(android.R.drawable.ic_dialog_alert);
			alert.show();
			return true;
		}
		return false;
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		super.onActivityResult(requestCode, resultCode, data);

		switch (requestCode) {
		case RESULT_FIRST_USER:
			SharedPreferences prefs = PreferenceManager
					.getDefaultSharedPreferences(this);
			default_deadline = Integer.parseInt(prefs.getString("prefDeadline",
					Integer.toString(default_deadline)));
			break;
		}
	}

	/***************************************************************************/

	@SuppressLint("HandlerLeak")
	public void createHandler() {
		/* defines handler for communication between threads */
		main_handle = new Handler() {
			@Override
			public void handleMessage(Message msg) {
				super.handleMessage(msg);

				switch (msg.what) {
				
				case SYSTEM_SHUTDOWN:
					if (mService != null)
						try {
							mService.systemShutdown();
						} catch (RemoteException e) {
							// TODO Auto-generated catch block
							e.printStackTrace();
						}
					break;

				case LIST_UPDATED:
					updateRecent();
					refreshChannelTab();
					break;

				case CONN_CLOSED:
					updateRecent();
					refreshChannelTab();

					String title = msg.getData().getString("title");
					String file_name = msg.getData().getString("file_name");
					String file_type = msg.getData().getString("file_type");
					if (file_type == null) {
						FileNameMap fileNameMap = URLConnection
								.getFileNameMap();
						file_type = fileNameMap
								.getContentTypeFor(ReadyCastFileIO.mSdPath
										+ "/ReadyCast/" + file_name);
						if (file_type.contains("video"))
							file_type = "video/*";
					}
					Notification notify_msg = new Notification(R.drawable.icon,
							title + " : Download Completed!",
							System.currentTimeMillis());
					// pop
					Intent notificationIntent = new Intent(MainActivity.this,
							MainActivity.class);
					notificationIntent = notificationIntent
							.setAction(MainActivity.AUDIO_SERVICE);
					notificationIntent = notificationIntent.putExtra("path",
							file_name);
					notificationIntent = notificationIntent.putExtra("type",
							file_type);
					notificationIntent = notificationIntent.putExtra("title",
							title);

					PendingIntent contentIntent = PendingIntent.getActivity(
							MainActivity.this, 0, notificationIntent,
							PendingIntent.FLAG_UPDATE_CURRENT);
					notify_msg.setLatestEventInfo(
							MainActivity.this.getApplicationContext(),
							"Download Completed!", title, contentIntent);
					notifier.notify(1, notify_msg);

					/* save current status to file */
					ReadyCastFileIO.saveToFile(feedlist);

					break;

				}
			}
		};
	}

	public void refreshChannelTab() {
		if (getActionBar().getSelectedNavigationIndex() == 0) {
			Tab1_Fragment a = (Tab1_Fragment) current_fragment;
			if (a.rssadapter != null)
				a.rssadapter.notifyDataSetChanged();
		}
	}
}
