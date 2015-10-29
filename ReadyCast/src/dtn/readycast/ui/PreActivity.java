package dtn.readycast.ui;

import java.util.ArrayList;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.DialogInterface.OnDismissListener;
import android.content.Intent;
import android.graphics.Typeface;
import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.animation.AnimationUtils;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.PopupWindow;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ViewSwitcher;
import dtn.readycast.R;
import dtn.readycast.ReadyCastFileIO;
import dtn.readycast.item.RSSFeed;

public class PreActivity extends FragmentActivity {
	private ViewSwitcher vs;
	int depth = 0;
	private ListView lv, lv2;
	private TextView lv2_category;

	public ImageView imageview;
	public TextView title, description;
	public Button button;
	public ListView recentlist;

	public Activity pre_activity = this;

	EditText inputSearch;
	ArrayAdapter<String> array, array2;
	FilterAdapter full_array;

	ArrayList<String> full_title = new ArrayList<String>();
	ArrayList<String> full_id = new ArrayList<String>();
	ArrayList<RSSFeed> list = new ArrayList<RSSFeed>();

	String categoryList[] = {};
	String podcastTitle[] = {};
	String podcastID[] = {};

	Button btnClosePopup;
	private PopupWindow pwindo;

	Fragment current_fragment = new Tab3_Fragment();
	Handler h;

	int numFeed = 0;

	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if (keyCode == KeyEvent.KEYCODE_BACK) {
			if (pwindo.isShowing()) {
				pwindo.dismiss();
				return false;
			}
			if (depth > 0) {
				vs.setInAnimation(AnimationUtils.loadAnimation(this,
						R.anim.slide_in_left));
				vs.setOutAnimation(AnimationUtils.loadAnimation(this,
						R.anim.slide_out_right));
				vs.showPrevious();
				depth--;
				return true;
			}

		}
		return super.onKeyDown(keyCode, event);
	}

	@SuppressLint("InflateParams")
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		h = new Handler();
		initiatePopupWindow();

		LayoutInflater inflater = getLayoutInflater();
		View myFragmentView = inflater.inflate(R.layout.tab3_view, null);
		vs = (ViewSwitcher) myFragmentView.findViewById(R.id.viewswitcher);
		lv = (ListView) myFragmentView.findViewById(R.id.ituneslist);
		lv2 = (ListView) myFragmentView.findViewById(R.id.detaillist);
		lv2_category = (TextView) myFragmentView
				.findViewById(R.id.chosen_category);

		inputSearch = (EditText) myFragmentView.findViewById(R.id.inputSearch);
		getWindow().setSoftInputMode(
				WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);

		categoryList = this.getResources().getStringArray(R.array.itunes_title);
		array = new ArrayAdapter<String>(pre_activity,
				android.R.layout.simple_list_item_1, categoryList);

		// create full list of all feeds
		for (int i = 1; i < categoryList.length; i++) {
			podcastTitle = pre_activity.getResources().getStringArray(
					resrc_title[i]);
			podcastID = pre_activity.getResources().getStringArray(resrc_id[i]);
			for (int j = 0; j < podcastTitle.length; j++) {
				full_title.add(podcastTitle[j]);
				full_id.add(podcastID[j]);
			}
		}
		full_array = new FilterAdapter(full_title);

		// YHWAN: read feed list
		ReadyCastFileIO.loadFromFile(null, list);
		numFeed = list.size();

		lv.setTextFilterEnabled(true);
		lv.setAdapter(array);
		lv.setOnItemClickListener(new OnItemClickListener() {
			@Override
			public void onItemClick(AdapterView<?> arg0, View v, int position,
					long id) {
				if (lv.getAdapter() != array) {

					Log.d("appdtp", "OnItemClick");

					ArrayList<String> filtered_title = full_array
							.getFilteredList();

					/* get id */
					String filtered_id = null;
					for (int i = 0; i < full_title.size(); i++) {
						if (filtered_title.get(position).equals(
								full_title.get(i))) {
							filtered_id = full_id.get(i);
							break;
						}
					}

					Dialog dialog = new Dialog(pre_activity,
							android.R.style.Theme_Holo_Light_DialogWhenLarge);
					dialog.setContentView(R.layout.tab3_dialog);
					dialog.setTitle(R.string.tab3_title);
					new Tab3_SubDialog(filtered_title.get(position), dialog,
							(Tab3_Fragment) current_fragment, 1)
							.execute(filtered_id);
					dialog.setOnDismissListener(new OnDismissListener() {
						@Override
						public void onDismiss(DialogInterface dialog) {
							list.clear();
							ReadyCastFileIO.loadFromFile(null, list);
							if (list.size() + 1 >= 5) {
								Toast toast2 = Toast.makeText(PreActivity.this,
										"Subscription Completed!",
										Toast.LENGTH_SHORT);
								toast2.show();
								h.post(r);
							} else {
								if (list.size() > numFeed) {
									Toast.makeText(PreActivity.this,
											"Feed Added!", Toast.LENGTH_SHORT)
											.show();
									numFeed = list.size();
								}
							}
						}
					});
					dialog.show();
				} else {
					depth++;
					updateArray2(position);

					vs.setInAnimation(AnimationUtils.loadAnimation(
							pre_activity, R.anim.slide_in_right));
					vs.setOutAnimation(AnimationUtils.loadAnimation(
							pre_activity, R.anim.slide_out_left));
					vs.showNext();
				}
			}
		});

		lv2.setOnItemClickListener(new OnItemClickListener() {
			@Override
			public void onItemClick(AdapterView<?> arg0, View v, int position,
					long id) {
				Dialog dialog = new Dialog(pre_activity,
						android.R.style.Theme_Holo_Light_DialogWhenLarge);
				dialog.setContentView(R.layout.tab3_dialog);
				dialog.setTitle(R.string.tab3_title);
				new Tab3_SubDialog(podcastTitle[position], dialog,
						(Tab3_Fragment) current_fragment, 1)
						.execute(podcastID[position]);
				dialog.setOnDismissListener(new OnDismissListener() {
					@Override
					public void onDismiss(DialogInterface dialog) {
						list.clear();
						ReadyCastFileIO.loadFromFile(null, list);
						if (list.size() >= 5) {
							Toast.makeText(PreActivity.this,
									"Subscriptions Completed!",
									Toast.LENGTH_SHORT).show();
							h.post(r);
						} else {
							if (list.size() > numFeed) {
								Toast.makeText(PreActivity.this, "Feed Added!",
										Toast.LENGTH_SHORT).show();
								numFeed = list.size();
							}
						}
					}
				});
				dialog.show();
			}
		});

		inputSearch.addTextChangedListener(new TextWatcher() {
			@Override
			public void onTextChanged(CharSequence cs, int start, int before,
					int count) {
				if (start == 0 && count == 0) {
					lv.setAdapter(array);
				} else {
					lv.setAdapter(full_array);
					full_array.getFilter().filter(cs);
				}
			}

			@Override
			public void beforeTextChanged(CharSequence cs, int start,
					int count, int after) {
			}

			@Override
			public void afterTextChanged(Editable s) {
			}

		});

		setContentView(myFragmentView);
	}

	Runnable r = new Runnable() {
		public void run() {
			Intent i = new Intent(PreActivity.this, MainActivity.class);
			startActivity(i);
			finish();

			overridePendingTransition(android.R.anim.fade_in,
					android.R.anim.fade_out);
		}
	};

	@SuppressWarnings("deprecation")
	private void initiatePopupWindow() {
		LayoutInflater inflater = getLayoutInflater();
		final View layout = inflater.inflate(R.layout.pre_view,
				(ViewGroup) findViewById(R.id.popup_element));

		int width = (int) getWindowManager().getDefaultDisplay().getWidth() * 4 / 5;
		pwindo = new PopupWindow(layout, width,
				WindowManager.LayoutParams.WRAP_CONTENT, true);
		pwindo.setBackgroundDrawable(new BitmapDrawable());
		pwindo.setOutsideTouchable(true);
		layout.post(new Runnable() {
			public void run() {
				pwindo.showAtLocation(layout, Gravity.CENTER, 0, 0);
			}
		});

		btnClosePopup = (Button) layout.findViewById(R.id.btn_close_popup);
		btnClosePopup.setOnClickListener(cancel_button_click_listener);
	}

	private OnClickListener cancel_button_click_listener = new OnClickListener() {
		public void onClick(View v) {
			pwindo.dismiss();
		}
	};

	public void updateArray2(int position) {
		podcastTitle = pre_activity.getResources().getStringArray(
				resrc_title[position]);
		podcastID = pre_activity.getResources().getStringArray(
				resrc_id[position]);
		array2 = new ArrayAdapter<String>(pre_activity,
				android.R.layout.simple_list_item_1, podcastTitle);
		lv2.setAdapter(array2);
		lv2_category.setText(categoryList[position]);
		lv2_category.setTypeface(null, Typeface.BOLD);
	}

	int resrc_id[] = { R.array.recommends_id, R.array.art_id,
			R.array.business_id, R.array.comedy_id, R.array.edu_id,
			R.array.game_id, R.array.govern_id, R.array.health_id,
			R.array.kids_id, R.array.music_id, R.array.news_id,
			R.array.religion_id, R.array.science_id, R.array.social_id,
			R.array.sports_id, R.array.tech_id, R.array.tv_movie_id };

	int resrc_title[] = { R.array.recommends_title, R.array.art_title,
			R.array.business_title, R.array.comedy_title, R.array.edu_title,
			R.array.game_title, R.array.govern_title, R.array.health_title,
			R.array.kids_title, R.array.music_title, R.array.news_title,
			R.array.religion_title, R.array.science_title,
			R.array.social_title, R.array.sports_title, R.array.tech_title,
			R.array.tv_movie_title };
}
