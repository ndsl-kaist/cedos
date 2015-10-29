package dtn.readycast.ui;

import java.util.ArrayList;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.graphics.Typeface;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AnimationUtils;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.ViewSwitcher;
import dtn.readycast.R;

public class Tab3_Fragment extends Fragment {

	private ViewSwitcher vs;
	int depth = 0;
	private ListView lv, lv2;
	private TextView lv2_category;

	public ImageView imageview;
	public TextView title, description;
	public Button button;
	public ListView recentlist;

	public Activity main_activity;
	EditText inputSearch;
	ArrayAdapter<String> array, array2;
	FilterAdapter full_array;

	ArrayList<String> full_title = new ArrayList<String>();
	ArrayList<String> full_id = new ArrayList<String>();

	String categoryList[] = {};
	String podcastTitle[] = {};
	String podcastID[] = {};

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);
		main_activity = activity;
	}

	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if (keyCode == KeyEvent.KEYCODE_BACK) {
			if (depth > 0) {
				vs.setInAnimation(AnimationUtils.loadAnimation(main_activity,
						R.anim.slide_in_left));
				vs.setOutAnimation(AnimationUtils.loadAnimation(main_activity,
						R.anim.slide_out_right));
				vs.showPrevious();
				depth--;
				return true;
			} else
				return false;
		}
		return false;
	}

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		depth = 0;

		Log.d("appdtp", "CategoryListActivity : oncreate()");
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// TODO Auto-generated method stub
		View myFragmentView = inflater.inflate(R.layout.tab3_view, container,
				false);

		vs = (ViewSwitcher) myFragmentView.findViewById(R.id.viewswitcher);
		lv = (ListView) myFragmentView.findViewById(R.id.ituneslist);
		lv2 = (ListView) myFragmentView.findViewById(R.id.detaillist);
		lv2_category = (TextView) myFragmentView
				.findViewById(R.id.chosen_category);

		inputSearch = (EditText) myFragmentView.findViewById(R.id.inputSearch);
		InputMethodManager imm = (InputMethodManager) main_activity
				.getSystemService(Context.INPUT_METHOD_SERVICE);
		imm.hideSoftInputFromWindow(myFragmentView.getWindowToken(), 0);

		categoryList = getActivity().getResources().getStringArray(
				R.array.itunes_title);
		array = new ArrayAdapter<String>(main_activity,
				android.R.layout.simple_list_item_1, categoryList);

		// create full list of all feeds
		for (int i = 1; i < categoryList.length; i++) {
			podcastTitle = main_activity.getResources().getStringArray(
					resrc_title[i]);
			podcastID = main_activity.getResources()
					.getStringArray(resrc_id[i]);
			for (int j = 0; j < podcastTitle.length; j++) {
				full_title.add(podcastTitle[j]);
				full_id.add(podcastID[j]);
			}
		}
		full_array = new FilterAdapter(full_title);

		lv.setAdapter(array);
		lv.setOnItemClickListener(new OnItemClickListener() {
			@Override
			public void onItemClick(AdapterView<?> arg0, View v, int position,
					long id) {
				if (lv.getAdapter() != array) {
					ArrayList<String> filtered_title = full_array
							.getFilteredList();

					/* find offset for title */
					String filtered_id = null;
					for (int i = 0; i < full_title.size(); i++) {
						if (filtered_title.get(position).equals(
								full_title.get(i))) {
							filtered_id = full_id.get(i);
							break;
						}
					}

					Dialog dialog = new Dialog(main_activity,
							android.R.style.Theme_Holo_Light_DialogWhenLarge);
					dialog.setContentView(R.layout.tab3_dialog);
					dialog.setTitle(R.string.tab3_title);
					new Tab3_SubDialog(filtered_title.get(position), dialog,
							Tab3_Fragment.this, 0).execute(filtered_id);
					dialog.show();
				} else {
					depth++;
					updateArray2(position);

					vs.setInAnimation(AnimationUtils.loadAnimation(
							main_activity, R.anim.slide_in_right));
					vs.setOutAnimation(AnimationUtils.loadAnimation(
							main_activity, R.anim.slide_out_left));
					vs.showNext();
				}
			}
		});

		lv2.setOnItemClickListener(new OnItemClickListener() {
			@Override
			public void onItemClick(AdapterView<?> arg0, View v, int position,
					long id) {
				Dialog dialog = new Dialog(main_activity,
						android.R.style.Theme_Holo_Light_DialogWhenLarge);
				dialog.setContentView(R.layout.tab3_dialog);
				dialog.setTitle(R.string.tab3_title);
				new Tab3_SubDialog(podcastTitle[position], dialog,
						Tab3_Fragment.this, 0).execute(podcastID[position]);
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
					Tab3_Fragment.this.full_array.getFilter().filter(cs);
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

		return myFragmentView;
	}

	public void updateArray2(int position) {
		podcastTitle = main_activity.getResources().getStringArray(
				resrc_title[position]);
		podcastID = main_activity.getResources().getStringArray(
				resrc_id[position]);
		array2 = new ArrayAdapter<String>(main_activity,
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
