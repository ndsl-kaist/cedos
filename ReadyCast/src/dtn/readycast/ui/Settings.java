package dtn.readycast.ui;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceActivity;
import dtn.readycast.R;

public class Settings extends PreferenceActivity {
	OnPreferenceChangeListener listener;
	OnPreferenceClickListener click_listener;

	@SuppressWarnings("deprecation")
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		addPreferencesFromResource(R.layout.settings);

		Preference intro = findPreference("intro");
		intro.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {

					@Override
					public boolean onPreferenceClick(Preference preference) {
						Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://www.ndsl.kaist.edu/ReadyCast/"));
						startActivity(browserIntent);
						return true;
					}
				});

		Preference help = findPreference("help");
		help.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {

					@Override
					public boolean onPreferenceClick(Preference preference) {
						Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://www.ndsl.kaist.edu/ReadyCast/help.html"));
						startActivity(browserIntent);
						return true;
					}
				});
		
		Preference survey = findPreference("survey");
		survey.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {

					@Override
					public boolean onPreferenceClick(Preference preference) {
						Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://cedos.kaist.edu"));
						startActivity(browserIntent);
						return true;
					}
				});
		Preference email = findPreference("email");
		email.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {

					@Override
					public boolean onPreferenceClick(Preference preference) {
						Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("mailto:ygmoon@ndsl.kaist.edu"));
						startActivity(browserIntent);
						return true;
					}
				});		
		
	}
}