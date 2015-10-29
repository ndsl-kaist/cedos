package dtn.readycast.ui;

import java.util.ArrayList;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.view.Menu;
import android.view.Window;
import dtn.readycast.R;
import dtn.readycast.ReadyCastFileIO;
import dtn.readycast.item.RSSFeed;

public class IntroActivity extends Activity {

	Handler h;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		setContentView(R.layout.main_intro);
		h = new Handler();
		h.postDelayed(r, 2000);

		super.onCreate(savedInstanceState);
	}

	Runnable r = new Runnable() {
		public void run() {
			Intent i;
			ArrayList<RSSFeed> list = new ArrayList<RSSFeed>();
			ReadyCastFileIO.loadFromFile(null, list);

			i = new Intent(IntroActivity.this, MainActivity.class);
			startActivity(i);
			finish();

			overridePendingTransition(android.R.anim.fade_in,
					android.R.anim.fade_out);
		}
	};

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.layout.main_intro, menu);
		return true;
	}
}
