package dtn.readycast.ui;

import java.io.File;
import java.net.FileNameMap;
import java.net.URLConnection;
import java.util.ArrayList;

import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.PorterDuff.Mode;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.util.Log;
import android.util.SparseBooleanArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TabHost;
import android.widget.TextView;
import android.widget.Toast;
import dtn.readycast.R;
import dtn.readycast.ReadyCastFileIO;
import dtn.readycast.item.RSSFeed;
import dtn.readycast.item.RSSItem;

public class Tab1_SubDialog extends Dialog {

	private Context context;
	public Spinner s;
	public ArrayAdapter<String> spinadapter = null;
	public RSSFeed feed;

	public ListView lv;
	public ArrayList<RSSItem> list_all = new ArrayList<RSSItem>();
	public Tab1_SubListAdapter adapter;
	public Tab1_SubListDeleteAdapter adapter_delete;

	public ListView lv2;
	public ArrayList<String> list2 = new ArrayList<String>();
	public ArrayList<RSSItem> list_all2 = new ArrayList<RSSItem>();
	public ArrayAdapter<String> adapter2;

	public AlertDialog levelDialog;

	@SuppressWarnings("deprecation")
	public Tab1_SubDialog(final Context _context, int theme, RSSFeed _feed) {
		super(_context, theme);
		context = _context;
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		feed = _feed;
		setContentView(R.layout.tab1_dialog);

		lv = (ListView) findViewById(R.subscr.list1);
		adapter = new Tab1_SubListAdapter(getContext(), R.tdi1.txtTitle,
				list_all);

		adapter_delete = new Tab1_SubListDeleteAdapter(getContext(),
				R.tdi1.txtTitle, list_all);
		Button b_delete = (Button) findViewById(R.subscr.btnDeleteItem);
		Button b_selectall = (Button) findViewById(R.subscr.btnSelectAll);

		lv2 = (ListView) findViewById(R.subscr.list2);
		adapter2 = new ArrayAdapter<String>(getContext(),
				R.layout.tab1_dialog_item2, list2);
		ImageView imageview = (ImageView) findViewById(R.subscr.imageView);
		TextView titleview = (TextView) findViewById(R.subscr.title);
		TabHost t = (TabHost) findViewById(R.subscr.tabs);
		s = (Spinner) findViewById(R.subscr.spinner);
		TextView downloadnum = (TextView) findViewById(R.subscr.txtDownloading);
		LinearLayout downloadbtn = (LinearLayout) findViewById(R.subscr.btnDownloading);
		LinearLayout deleteModebtn = (LinearLayout) findViewById(R.subscr.btnDeleteMode);

		Button b_now = (Button) findViewById(R.subscr.btnNow);
		Button b_delay = (Button) findViewById(R.subscr.btnDelay);

		t.setup();
		t.addTab(t.newTabSpec("tab1")
				.setIndicator(getContext().getString(R.string.tab1_1_title))
				.setContent(R.subscr.tab1));
		t.addTab(t.newTabSpec("tab2")
				.setIndicator(getContext().getString(R.string.tab1_2_title))
				.setContent(R.subscr.tab2));
		t.setCurrentTab(0);
		titleview.setText(feed.title);

		Bitmap myBitmap = BitmapFactory.decodeFile(Environment
				.getExternalStorageDirectory().getPath()
				+ "/ReadyCast/thumb_image/" + feed.itunes_id + ".jpg");
		imageview.setImageBitmap(myBitmap);

		lv.setAdapter(adapter);
		lv2.setAdapter(adapter2);
		refreshListData();
		refreshListData2();

		TabHost tabs = (TabHost) findViewById(R.subscr.tabs);
		
		tabs.setOnTabChangedListener(new TabHost.OnTabChangeListener() {

			@Override
			public void onTabChanged(String tabId) {

				if (tabId == "tab2") {
					SharedPreferences prefs = PreferenceManager
							.getDefaultSharedPreferences(_context);
					if (prefs.getBoolean("first_popup", true) || ((int)(Math.random() * 7.0)) == 0) {
						AlertDialog popup_window = null;
						
						AlertDialog.Builder popup_builder = new AlertDialog.Builder(
								getContext());
						popup_builder.setTitle(R.string.tab1_popupTitle);
						popup_builder.setMessage(R.string.tab1_popupMessage);
	
						popup_builder.setPositiveButton("OK", null);
						popup_window = popup_builder.create();
						popup_window.show();
						prefs.edit().putBoolean("first_popup", false).commit();
					}
					
				}

			}
		});

		TextView sched = (TextView) findViewById(R.subscr.subscr);
		sched.setText(feed.getScheduleText(getContext()));

		downloadnum.setText(feed.getNumDownloadingText(getContext(), false));
		downloadbtn.setOnClickListener(new android.view.View.OnClickListener() {
			@Override
			public void onClick(View v) {
				// TODO Auto-generated method stub
				dismiss();
				((MainActivity) context).refreshCurrentTab2();
			}
		});

		deleteModebtn
		.setOnClickListener(new android.view.View.OnClickListener() {
			@Override
			public void onClick(View v) {
				ImageView modeImageView = (ImageView) findViewById(R.subscr.imgDeletemode);
				TextView modeTextView = (TextView) findViewById(R.subscr.txtDeletemode);

				if (lv.getAdapter() == adapter) {
					modeImageView.setVisibility(View.GONE);
					modeTextView.setText(R.string.tab1_1_btn1);
					lv.setAdapter(adapter_delete);
					lv.setChoiceMode(AbsListView.CHOICE_MODE_MULTIPLE);
					LinearLayout l = (LinearLayout) findViewById(R.subscr.lower_menu1);
					l.setVisibility(View.VISIBLE);
				} else {
					LinearLayout l = (LinearLayout) findViewById(R.subscr.lower_menu1);
					l.setVisibility(View.GONE);
					modeImageView.setVisibility(View.VISIBLE);
					modeTextView.setText(R.string.tab1_1_btn);
					lv.setAdapter(adapter);
				}
			}
		});

		android.view.WindowManager.LayoutParams params = getWindow()
				.getAttributes();
		params.height = android.view.ViewGroup.LayoutParams.MATCH_PARENT;
		getWindow().setAttributes(
				params);

		lv2.setChoiceMode(AbsListView.CHOICE_MODE_MULTIPLE);

		// Load the icon as drawable object
		Drawable d = getContext().getResources().getDrawable(
				R.drawable.img_schedule_holo_blue);
		// Get the color of the icon depending on system state
		int iconColor = android.graphics.Color.parseColor("#0099CC");
		// Set the correct new color
		d.setColorFilter(iconColor, Mode.MULTIPLY);

		ArrayList<String> spinArray = new ArrayList<String>();
		spinArray.add("within 1 hours");
		spinArray.add("within 3 hours");
		spinArray.add("within 6 hours");
		spinArray.add("within 12 hours");
		spinadapter = new ArrayAdapter<String>(getContext(),
				R.layout.tab1_dialog_spinner, spinArray);
		s.setAdapter(spinadapter);

		lv.setOnItemClickListener(new OnItemClickListener() {
			@Override
			public void onItemClick(AdapterView<?> arg0, View arg1, int arg2,
					long arg3) {
				if (lv.getAdapter() == adapter) {
					RSSItem data = list_all.get(arg2);
					playFile(data);
					list_all.get(arg2).status = RSSItem.DOWNLOADED_PLAYED;
					((MainActivity) context).updateRecent();
					adapter.notifyDataSetChanged();
					((MainActivity) context).refreshChannelTab();
					ReadyCastFileIO.saveFeedToFile(data.parentFeed);
				}
			}
		});

		lv2.setOnItemClickListener(new OnItemClickListener() {

			@Override
			public void onItemClick(AdapterView<?> arg0, View arg1, int arg2,
					long arg3) {
				LinearLayout l = (LinearLayout) findViewById(R.subscr.lower_menu2);
				if (lv2.getCheckedItemCount() > 0)
					l.setVisibility(View.VISIBLE);
				else
					l.setVisibility(View.GONE);
			}
		});

		b_delete.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				Tab1_Fragment main_fragment = (Tab1_Fragment) ((MainActivity) context).current_fragment;
				SparseBooleanArray checkedItems = lv.getCheckedItemPositions();
				for (int i = 0; i < checkedItems.size(); i++) {
					if (checkedItems.valueAt(i)) {
						main_fragment.deleteFile(list_all.get(checkedItems
								.keyAt(i)));
					}
				}
				if (checkedItems.size() > 0) {
					LinearLayout l = (LinearLayout) findViewById(R.subscr.lower_menu1);
					refreshALL();
					l.setVisibility(View.GONE);
					lv.setAdapter(adapter_delete);
				}
			}
		});

		b_selectall.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				// select all
				for (int i = 0; i < lv.getCount(); i++)
					lv.setItemChecked(i, true);
			}
		});

		b_now.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				// TODO Auto-generated method stub
				SparseBooleanArray checkedItems = lv2.getCheckedItemPositions();
				for (int i = 0; i < checkedItems.size(); i++) {
					if (checkedItems.valueAt(i)) {
						Log.d("appdtp",
								"checked item: "
										+ lv2.getItemAtPosition(checkedItems
												.keyAt(i)));
						list_all2.get(checkedItems.keyAt(i)).deadline_sec = (System
								.currentTimeMillis() / 1000);
						list_all2.get(checkedItems.keyAt(i)).startDownload(
								(MainActivity) context);
						
					}
				}
				/***************************************************/
				LayoutInflater inflater = getLayoutInflater();
				View layout = inflater.inflate(R.layout.toast_layout,
				                               (ViewGroup) findViewById(R.id.toast_layout_root));

				TextView text = (TextView) layout.findViewById(R.id.text);
				Toast toast = new Toast(_context);
				switch ((int)(Math.random() * 3)) {
				case 0:
					text.setText(R.string.tab1_helpMessage1);
					break;
				case 1:
					text.setText(R.string.tab1_helpMessage2);
					break;
				case 2:
					text.setText(R.string.tab1_helpMessage3);
					break;
				default:
					break;
				}
				toast.setDuration(Toast.LENGTH_LONG);
				toast.setView(layout);
				toast.show();
				/***************************************************/
				
				refreshListData2();
				dismiss();
				((MainActivity) context).refreshCurrentTab2();
			}
		});

		b_delay.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {

				if (((MainActivity) context).default_deadline > 0) {
					SparseBooleanArray checkedItems = lv2
							.getCheckedItemPositions();
					for (int i = 0; i < checkedItems.size(); i++) {
						if (checkedItems.valueAt(i)) {
							Log.d("appdtp",
									"checked item: "
											+ lv2.getItemAtPosition(checkedItems
													.keyAt(i)));
							list_all2.get(checkedItems.keyAt(i)).deadline_sec = (System
									.currentTimeMillis() / 1000)
									+ ((MainActivity) context).default_deadline * 60;
							list_all2.get(checkedItems.keyAt(i)).startDownload(
									(MainActivity) context);
						}
					}
					refreshListData2();
					dismiss();
					((MainActivity) context).refreshCurrentTab2();
				} else {
					levelDialog = null;

					// Strings to Show In Dialog with Radio Buttons
					final CharSequence[] items = {" 10 minutes ", " 30 minutes ", " 1 hour ", " 2 hours ", " 3 hours ", " 6 hours " };

					// Creating and Building the Dialog
					AlertDialog.Builder builder = new AlertDialog.Builder(
							getContext());
					builder.setTitle("I want to watch them after ...");
					builder.setSingleChoiceItems(items, -1,
							new DialogInterface.OnClickListener() {
						@Override
						public void onClick(DialogInterface dialog,
								int item) {
							int deadline_sec = 0;
							switch (item) {
							case 0:
								deadline_sec = 10 * 60;
								break;
							case 1:
								deadline_sec = 30 * 60;
								break;
							case 2:
								deadline_sec = 60 * 60;
								break;
							case 3:
								deadline_sec = 2 * 60 * 60;
								break;
							case 4:
								deadline_sec = 3 * 60 * 60;
								break;
							case 5:
								deadline_sec = 6 * 60 * 60;
								break;
							}
							levelDialog.dismiss();

							SparseBooleanArray checkedItems = lv2
									.getCheckedItemPositions();
							for (int i = 0; i < checkedItems.size(); i++) {
								if (checkedItems.valueAt(i)) {
									Log.d("appdtp",
											"checked item: "
													+ lv2.getItemAtPosition(checkedItems
															.keyAt(i)));
									list_all2.get(checkedItems.keyAt(i)).deadline_sec = (System
											.currentTimeMillis() / 1000)
											+ deadline_sec;
									list_all2
									.get(checkedItems.keyAt(i))
									.startDownload(
											(MainActivity) context);
								}
							}

							/*****************************************************************/
							LayoutInflater inflater = getLayoutInflater();
							View layout = inflater.inflate(R.layout.toast_layout,
							                               (ViewGroup) findViewById(R.id.toast_layout_root));

							TextView text = (TextView) layout.findViewById(R.id.text);
							Toast toast = new Toast(_context);
							switch ((int)(Math.random() * 2)) {
							case 0:
								text.setText(R.string.tab1_helpMessage4);
								break;
							case 1:
								text.setText(R.string.tab1_helpMessage5);
								break;
							default:
								break;
							}
							toast.setDuration(Toast.LENGTH_LONG);
							toast.setView(layout);
							toast.show();
							/*****************************************************************/

							refreshListData2();
							dismiss();
							((MainActivity) context)
							.refreshCurrentTab2();
						}
					});

					levelDialog = builder.create();
					levelDialog.show();
				}

			}
		});
	}

	public void refreshALL() {
		refreshListData();
		refreshListData2();

		TextView downloadnum = (TextView) findViewById(R.subscr.txtDownloading);
		downloadnum.setText(feed.getNumDownloadingText(getContext(), false));
	}

	public void refreshListData() {
		int i;
		list_all.clear();

		for (i = 0; i < feed.items.size(); i++) {
			if (feed.items.get(i).status == RSSItem.DOWNLOADED
					|| feed.items.get(i).status == RSSItem.DOWNLOADED_PLAYED) {
				list_all.add(feed.items.get(i));
			}
		}
		adapter.notifyDataSetChanged();
	}

	public void refreshListData2() {
		int i;
		list2.clear();
		list_all2.clear();

		for (i = 0; i < feed.items.size(); i++) {
			if (feed.items.get(i).status == RSSItem.NONE) {
				list2.add(feed.items.get(i).title);
				list_all2.add(feed.items.get(i));
			}
		}
		adapter2.notifyDataSetChanged();
	}

	@SuppressLint("SimpleDateFormat")
	public void playFile(final RSSItem data) {

		MainActivity main_activity = (MainActivity) context;

		try {
			
			if (data.file_type == null) {
				FileNameMap fileNameMap = URLConnection.getFileNameMap();
				data.file_type = fileNameMap
						.getContentTypeFor(ReadyCastFileIO.mSdPath
								+ "/ReadyCast/" + data.file_name);
				if (data.file_type.contains("video"))
					data.file_type = "video/*";
			}

			if (data.file_type != null) {
				Intent intent = new Intent(Intent.ACTION_VIEW);
				Uri uri = Uri.fromFile(new File(ReadyCastFileIO.mSdPath
						+ "/ReadyCast/" + data.file_name));
				intent.setDataAndType(uri, data.file_type);
				main_activity.startActivity(intent);
				Toast.makeText(main_activity, "Now Playing...\n" + data.title,
						Toast.LENGTH_SHORT).show();
			} else
				Toast.makeText(main_activity,
						"Unknown file type\n" + data.title, Toast.LENGTH_SHORT)
						.show();

		} catch (IllegalArgumentException e) {
			Toast.makeText(main_activity, "IllegalArgumentException",
					Toast.LENGTH_SHORT).show();
			e.printStackTrace();
		} catch (IllegalStateException e) {
			Toast.makeText(main_activity, "IllegalStateException",
					Toast.LENGTH_SHORT).show();
			e.printStackTrace();
		} catch (ActivityNotFoundException e) {
			Toast.makeText(
					main_activity,
					"Not playable in this device, please install another player "
							+ "which can support " + data.file_type
							+ " format.", Toast.LENGTH_SHORT).show();
			e.printStackTrace();
		}
	}
}
