package dtn.readycast.ui;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;

import android.app.Dialog;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.AsyncTask;
import android.os.Environment;
import android.text.Html;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;
import dtn.readycast.R;
import dtn.readycast.item.RSSRetrieveFeeds;
import dtn.readycast.item.RSSRetrieveFeedsInit;

public class Tab3_SubDialog extends AsyncTask<String, Void, String> {

	public static final String format = "https://itunes.apple.com/kr/podcast/id";
	Dialog dialog;
	String title;
	String image_url;
	String id;
	Tab3_Fragment act;
	int type;

	/* constructor for RetrieveRSSFeeds */
	public Tab3_SubDialog(String _title, Dialog _dialog, Tab3_Fragment _act,
			int _type) {
		title = _title;
		dialog = _dialog;
		act = _act;
		type = _type;
	}

	protected String doInBackground(String... params) {
		try {
			id = params[0];
			String url_in = format + params[0];
			HttpURLConnection urlConnection = null;
			String str = new String();
			int bytes_read;

			byte[] buffer = new byte[32 * 1024];
			try {
				URL url = new URL(url_in);
				urlConnection = (HttpURLConnection) url.openConnection();
				urlConnection.setInstanceFollowRedirects(true);
				urlConnection.connect();
				InputStream in = new BufferedInputStream(
						urlConnection.getInputStream());
				while (true) {
					if ((bytes_read = in.read(buffer)) == -1)
						break;
					str += new String(buffer, 0, bytes_read);
				}
			} catch (MalformedURLException e) {
				e.printStackTrace();
				str = null;
			} catch (IOException e) {
				e.printStackTrace();
				str = null;
			} finally {
				if (urlConnection != null)
					urlConnection.disconnect();
			}

			String result = GetImageURL(str);
			Log.d("appdtp", result);

			/*-----------------------------------------------------------------*/

			final File dir = new File(Environment.getExternalStorageDirectory()
					.getPath() + "/ReadyCast/thumb_image/");
			dir.mkdirs(); // create folders where write files
			File destinationFile = new File(dir, params[0] + ".jpg");

			if (!destinationFile.exists()) {

				BufferedOutputStream b = new BufferedOutputStream(
						new FileOutputStream(destinationFile));
				try {
					URL url = new URL(result);
					urlConnection = (HttpURLConnection) url.openConnection();
					urlConnection.setInstanceFollowRedirects(true);
					urlConnection.connect();
					InputStream in = new BufferedInputStream(
							urlConnection.getInputStream());
					while (true) {
						if ((bytes_read = in.read(buffer)) == -1)
							break;
						b.write(buffer, 0, bytes_read);
					}
				} catch (MalformedURLException e) {
					e.printStackTrace();
				} catch (IOException e) {
					e.printStackTrace();
				} finally {
					b.close();
					if (urlConnection != null)
						urlConnection.disconnect();
				}
			}
			/*-----------------------------------------------------------------*/

			return str;
		} catch (Exception e) {
			e.printStackTrace();
			return null;
		}
	}

	protected void onPostExecute(String s) {
		ImageView imageview = (ImageView) dialog
				.findViewById(R.podcast.imageView);
		TextView titleview = (TextView) dialog.findViewById(R.podcast.title);
		TextView description = (TextView) dialog
				.findViewById(R.podcast.description);
		Button button = (Button) dialog.findViewById(R.podcast.button);
		ListView ListView = (ListView) dialog
				.findViewById(R.podcast.recentlist);
		TextView notexist = (TextView) dialog.findViewById(R.podcast.not_exist);
		View loading = (View) dialog.findViewById(R.podcast.loading);
		String desc;

		loading.setVisibility(View.GONE);

		titleview.setText(title);

		if (act == null) {
			button.setText("구독 중");
			button.setEnabled(false);
		}

		button.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View v) {
				
				// XXX manually
				if (type == 0) {
					new RSSRetrieveFeeds(act.main_activity, -1)
							.execute(format + id);
				} else {
					new RSSRetrieveFeedsInit(-1)
							.execute(format + id);
				}
				dialog.dismiss();
			}
		});
		/*------------------------------------------------------------*/
		
		/* shows information only when metadata is retrieved appropriately */
		if (s != null) {
			if ((desc = GetDescription(s)) != null)
				description.setText(desc);
			if (SetRecentList(s, ListView) > 0) {
				ListView.setVisibility(View.VISIBLE);
				notexist.setVisibility(View.INVISIBLE);
			}
			Bitmap myBitmap = BitmapFactory.decodeFile(Environment
					.getExternalStorageDirectory().getPath()
					+ "/ReadyCast/thumb_image/" + id + ".jpg");
			imageview.setImageBitmap(myBitmap);
		}
	}

	private String GetImageURL(String s) {
		String prefix_image = "\"og:site_name\" /><meta content=\"";
		String postfix_image = "\" property=\"og:image\"";
		int a = (s.indexOf(prefix_image) + prefix_image.length());
		int b = (s.indexOf(postfix_image));
		return s.substring(a, b);
	}

	private String GetDescription(String ss) {
		if (ss == null)
			return null;
		int c = ss.indexOf("div metrics-loc=\"Titledbox_설명\"");
		if (c == -1)
			return null;

		String s = ss.substring(c); // check
		String prefix_image = "<p>";
		String postfix_image = "</p>";
		int a = (s.indexOf(prefix_image) + prefix_image.length());
		int b = (s.indexOf(postfix_image));
		return s.substring(a, b);
	}

	private int SetRecentList(String s, ListView listview) {
		String result = null;
		ArrayList<String> list = new ArrayList<String>();
		ArrayAdapter<String> adapter = new ArrayAdapter<String>(
				dialog.getContext(), R.layout.tab3_dialog_item, list);
		listview.setAdapter(adapter);

		int tr_index;
		String tr = "sort-value=\"";
		String prefix = "<span class=\"text\">";
		String postfix = "</span></span>";

		int index = 0;
		while (index < 10) {
			/*
			 * the starting part of item looks like <td role="gridcell"
			 * sort-value="num" class="index ascending">
			 */
			tr_index = s.indexOf(tr + (index + 1) + "\"");
			if (tr_index == -1)
				break;
			s = s.substring(tr_index + tr.length());

			int a = s.indexOf(prefix);
			if (a == -1)
				break;
			s = s.substring(a + prefix.length());
			int b = (s.indexOf(postfix));
			if (b == -1)
				break;
			result = s.substring(0, b);
			if (result == null)
				break;

			result = Html.fromHtml(result).toString();
			list.add(result);
			index += 1;
		}
		adapter.notifyDataSetChanged();

		return index;
	}
}
