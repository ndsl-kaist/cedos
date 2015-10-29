package dtn.net.service;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Locale;

import android.content.Context;
import android.content.Intent;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.WifiLock;
import android.os.PowerManager;
import android.os.RemoteException;
import android.util.Log;
import dtn.readycast.R;

public class DTPNetThread extends Thread {
	public static String DProxAddr;
	public static short DProxPort;
	public static int timeout = 1;
	public static final int MAX_RETRY = 5;
	public int isWaiting = 0;
	public boolean setInterrupt = false;
	
	public int bufsize = 32000;
	public byte[] read_buf = new byte[32000];

	public byte[] fd_set = new byte[1024];
	public int max_fd = 0;
	public ArrayList<DTPNetFlow> flow_set = null;
    private String deviceid;
    public String hostid;
    private Context ctx;
    
    WifiManager wm;
    PowerManager powerMgr;
    
	private WifiLock m_wlWifiLock = null;
	private PowerManager.WakeLock m_wlWakeLock = null;
    
	public DTPNetThread (Context _ctx, ArrayList<DTPNetFlow> _f, String _deviceid, WifiManager _wm, PowerManager _powerMgr) {
		ctx = _ctx;
		flow_set = _f;
		deviceid = _deviceid;
		wm = _wm;
		powerMgr = _powerMgr;

		DProxAddr = ctx.getResources().getString(R.string.dprox_ip);
		DProxPort = Short.parseShort(ctx.getResources().getString(R.string.dprox_port));
	}

	private void registerSock (int i) {
		if (i == -1)
			return;		
		
		fd_set[i] = 1;
		if (max_fd < i)
			max_fd = i;
	}
	private void unregisterSock (int i) {
		if (i == -1)
			return;
		
		fd_set[i] = 0;
		if (max_fd == i) {
			while(fd_set[max_fd] == 0) {
				max_fd--;
				if (max_fd < 0)
					break;
			}
		}
		
		/* alarms DTPNetAlarm to release cpu lock */
		ctx.sendBroadcast(new Intent(DTPWakeupStateMachine.ACTION_DTP_ALARM));
		
		//SendLogToServer();

	}
	private DTPNetFlow getFlowBySock (int sock) {
		int i;
		for (i = 0; i < flow_set.size(); i++) {
			if (sock == flow_set.get(i).socket)
				return flow_set.get(i);
		}
		return null;
	}
	public void addNewConnection (DTPNetFlow f) {
		f.socket = dtpsocket();
		registerSock(f.socket);
		//dtpsetsockopt(f.socket, 0, DTP_SO_RCVBUF, 256*1024);

		int untilDeadline = f.getTimeUntilDeadline();
		Log.d("appdtp", "until deadline = " + untilDeadline);
		dtpsetsockopt(f.socket, 0, DTP_SO_DEADLINE, (untilDeadline < 0)? 0 : untilDeadline);
		dtpconnect(f.socket, DProxAddr, DProxPort); // dprox address
		f.file_len = ((new File(f.file_path)).length());
		if (!SendRequestToDProx(f.socket, f.url, f.file_len, untilDeadline)) {
			Log.e("dtp_net", "SendRequestToDprox error!");
		}
		f.header = "";
		Log.e("dtp_net", "sock " + f.socket + " addNewConnection completed");
		f.status = 0;
	}

	public void run() {
		if( m_wlWifiLock == null ) {
			m_wlWifiLock = wm.createWifiLock("Downloader");
			m_wlWifiLock.setReferenceCounted(false);
			//m_wlWifiLock.acquire();
		}

		
		if( m_wlWakeLock == null ) {
			m_wlWakeLock = powerMgr.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "Downloader");
			//m_wlWakeLock.acquire();
		}		
		
		int k;
		for (k = 0; k < 1024; k++)
			fd_set[k] = 0;

		Log.d("dtp_net", "DTPNetThread start");
		while (true) {			
			int i, recv_len;
			DTPNetFlow flow;

			int j;
			for (j = 0; j < flow_set.size(); j++) {
				DTPNetFlow f = flow_set.get(j);
				if (f.status == 1) {
					addNewConnection(f);
				}
				else if (f.status == 2) {
					try {
						if (f.fout != null) {
							f.fout.flush();
							f.fout.close();
						}
					} catch (IOException e) {
						e.printStackTrace();
					}
					printAppLogging(f, (f.partialContentIndex.equals("-"))? 200:206);
					unregisterSock(f.socket);
					if (f.socket > 0)
						dtpclose(f.socket);
					flow_set.remove(f);
					DTPDownloadService.saveDownloadListToFile();
					if (flow_set.size() == 0) {
						CloseDTPNetThread();
						return;
					}
				}
			}
			
			if (this.setInterrupt) {
				Log.d("dtn_log", "setInterrupt is true!!!!");
				int idx;
				for (idx = 0; idx < flow_set.size(); idx++) {
					DTPNetFlow f = flow_set.get(idx);
					/* on System shutdown */
					try {
						if (f.fout != null) {
							f.fout.flush();
							f.fout.close();
						}
					} catch (IOException e) {
						e.printStackTrace();
					}
					printAppLogging(f, (f.partialContentIndex.equals("-"))? 200 : 206);
					if (f.socket > 0)
						dtpclose(f.socket);
					if (flow_set.size() == 0) {
						CloseDTPNetThread();
						return;
					}
				}
				DTPDownloadService.saveDownloadListToFile();
				return;
			}

			i = dtpselect(max_fd, fd_set, timeout);

			if (i == -2) {
				Log.d("dtp_net", "dtpselect timeout " + timeout);
				continue;
			}			
			else if (i == -1) {
				Log.e("dtp_net", "dtpselect error");
				CloseDTPNetThread();
				return;
			}

			/* dtpselect return value : i => 0 */
			flow = getFlowBySock(i);
			recv_len = dtpread(i, read_buf, bufsize);

			if (recv_len < 0) {
				Log.d("appdtp", "dtpread ERRNO = " + recv_len);
				if (recv_len == -11) {
					continue;
				}
				
				dtpclose(i);
				unregisterSock(i);
				Log.e("dtp_net", "reopen connection");

				printAppLogging(flow, (flow.partialContentIndex.equals("-"))? 200:206);
				/* XXX : REOPEN THE CONNECTION */
				addNewConnection(flow);

				continue;
			}
			else if (recv_len == 0) {
				try {
					if (flow.fout != null) {
						flow.fout.flush();
						flow.fout.close();
					}
					/*
					if (recv_len == 0 && flow.file != null) {
						Log.d("dtp_net", "md5sum = " + calculateMD5(flow.file)
								+ " / length = " + flow.file.length());
					}
					 */
				} catch (IOException e) {
					e.printStackTrace();
				}
				printAppLogging(flow, (flow.partialContentIndex.equals("-"))? 200:206);

				dtpclose(i);
				unregisterSock(i);

				Log.e("dtp_net", "close connection");

				try {
					if (flow.callback != null)
						flow.callback.valueChanged(100);
				} catch (RemoteException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
				flow_set.remove(flow);
				DTPDownloadService.saveDownloadListToFile();
				if (flow_set.size() == 0) {
					CloseDTPNetThread();
					return;
				}
				continue;
			}

			/* dtpread return value : i > 0 */

			if (!flow.isResponseParsed) {

				if (flow.fout == null) {
					try {
						flow.file = new File(flow.file_path);
						flow.fout = new BufferedOutputStream(new FileOutputStream(flow.file, /* append */
								true));
					} catch (FileNotFoundException e1) {
						// TODO Auto-generated catch block
						e1.printStackTrace();
					}

				}
				int enter = 0;
				String s = new String(read_buf);
				flow.header = flow.header + s.substring(0, (recv_len > s.length())? s.length(): recv_len);
				if ((enter = flow.header.indexOf("\n\n")) != -1) {
					enter += 2;
					flow.header = flow.header.substring(0, enter);
				} else if ((enter = flow.header.indexOf("\n\r\n")) != -1) {
					enter += 3;
					flow.header = flow.header.substring(0, enter);
				} else
					continue;

				/* substract header length to get payload length */
				recv_len -= enter;

				int code = parseStatusLine(flow.header);
				/* Malformed response header */
				if (code == -1) {
					Log.e("dtp_net", "malformed header!");
					break;
				}

				switch (code) {
				case 200: // OK
					if (flow.file_len > 0) {
						flow.file.delete();
						flow.file_len = 0;
					}
					break;
					
				case 206:
					String content_bytes = parseField(flow.header, HTTP_CONT_BYTES);
					flow.partialContentIndex = content_bytes;
					/* go on */
					break;

				case 301: // Moved Permanently
				case 302: // Moved Temporarily
					printAppLogging(flow, code);
					dtpclose(i);
					unregisterSock(i);


					/* reopen the connection */
					String location = parseField(flow.header, HTTP_LOCATION);
					flow.url = location;
					Log.d("dtp_net", "restart download (" + code + ") " + location);
					addNewConnection(flow);
					continue;

				case 404: // Not Found
					if (flow.callback != null)
						try {
							flow.callback.setNotFound();
						} catch (RemoteException e1) {
							// TODO Auto-generated catch block
							e1.printStackTrace();
						}
					printAppLogging(flow, code);
					dtpclose(i);
					unregisterSock(i);
					flow_set.remove(flow);
					DTPDownloadService.saveDownloadListToFile();
					if (flow_set.size() == 0) {
						CloseDTPNetThread();
						return;
					}
					continue;
				case 416:
					Log.d("appdtp", "### code = 416 dealt");
					Log.d("appdtp", flow.header);
					
					flow.file_len = new File(flow.file_path).length();
					String content_range = parseField(flow.header, HTTP_CONT_RANGE);
					if (content_range != null) {
						/* if it requested for fully downloaded file*/
						if (flow.file_len == Integer.parseInt(content_range)) {
							printAppLogging(flow, code);
							dtpclose(i);
							unregisterSock(i);
							flow_set.remove(flow);
							DTPDownloadService.saveDownloadListToFile();
							if (flow_set.size() == 0) {
								CloseDTPNetThread();
								return;
							}
							continue;
						}
					}
				case 504: // upstream connect timeout
					printAppLogging(flow, code);
					dtpclose(i);
					flow.retry_num++;
					Log.d("dtp_net", "### code = 504 dealt -- retry = "
							+ flow.retry_num);
					if (flow.retry_num > MAX_RETRY) {
						if (flow.callback != null)
							try {
								flow.callback.setNotFound();
							} catch (RemoteException e) {
								// TODO Auto-generated catch block
								e.printStackTrace();
							}
						unregisterSock(i);
						flow_set.remove(flow);
						DTPDownloadService.saveDownloadListToFile();
						if (flow_set.size() == 0) {
							CloseDTPNetThread();
							return;
						}
						continue;
					}

					i = dtpsocket();

					//dtpsetsockopt(i, 0, DTP_SO_RCVBUF, 256*1024);
					dtpconnect(i, DProxAddr, DProxPort); // dprox address
					flow.file_len = new File(flow.file_path).length();
					if (!SendRequestToDProx(i, flow.url, flow.file_len, 0)) {
						; /* XXX: notify error to UI */
						break;
					}
					flow.header = "";
					continue;
				default:
					if (flow.callback != null)
						try {
							flow.callback.setNotFound();
						} catch (RemoteException e1) {
							// TODO Auto-generated catch block
							e1.printStackTrace();
						}
					printAppLogging(flow, code);
					dtpclose(i);
					unregisterSock(i);
					flow_set.remove(flow);
					DTPDownloadService.saveDownloadListToFile();
					if (flow_set.size() == 0) {
						CloseDTPNetThread();
						return;
					}
					continue;
				}

				/* parse Content-length field */
				String content_length = parseField(flow.header, HTTP_CONT_LENGTH);
				if (content_length != null) {
					flow.total_len = flow.file_len + Integer.parseInt(content_length);

					try {
						if (flow.callback != null)
							flow.callback.setContentLength(flow.total_len);
					} catch (RemoteException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}

					Log.d("appdtp", "DTP_SO_BLOCKSIZE 1 : " + enter);
					Log.d("appdtp", "DTP_SO_BLOCKSIZE 2 : " + Integer.parseInt(content_length));
					dtpsetsockopt(i, 0, DTP_SO_BLOCKSIZE,
							enter + Integer.parseInt(content_length));

					/* alarms DTPNetAlarm to achieve cpu lock */
					ctx.sendBroadcast(new Intent(DTPWakeupStateMachine.ACTION_DTP_ALARM));
				}
				/* write remaining payload to file */
				try {
					flow.fout.write(read_buf, enter, recv_len);
					flow.file_len += recv_len;
					flow.down_len += recv_len;
				} catch (IOException e) {
					; /* XXX: deal with exception */
					e.printStackTrace();
					continue;
				}
				flow.isResponseParsed = true;
				Log.d("dtp_net", "response is parsed : " + flow.total_len);

				final int percent = (int) (flow.file_len * 100.0 / flow.total_len);
				if (flow.callback != null)
					try {
						flow.callback.valueChanged(percent);
					} catch (RemoteException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
				flow.prev_percent = percent;

				continue;
			}

			try {
				flow.fout.write(read_buf, 0, recv_len);
				flow.file_len += recv_len;
				flow.down_len += recv_len;
			} catch (IOException e) {
				; /* XXX: deal with not expected code */
				e.printStackTrace();
				continue;
			}

			/* XXX: STAT_UPDATE on percent change */
			final int percent = (int) (flow.file_len * 100.0 / flow.total_len);
			if (percent > flow.prev_percent) {
				if (flow.callback != null)
					try {
						flow.callback.valueChanged(percent);
					} catch (RemoteException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
						flow.callback = null;
					}
				flow.prev_percent = percent;
			}

		}		
	}

	public static String calculateMD5(File updateFile) {
		MessageDigest digest;
		try {
			digest = MessageDigest.getInstance("MD5");
		} catch (NoSuchAlgorithmException e) {
			Log.e("dtp_net", "Exception while getting Digest", e);
			return null;
		}

		InputStream is;
		try {
			is = new FileInputStream(updateFile);
		} catch (FileNotFoundException e) {
			Log.e("dtp_net", "Exception while getting FileInputStream", e);
			return null;
		}

		byte[] buffer = new byte[8192];
		int read;
		try {
			while ((read = is.read(buffer)) > 0) {
				digest.update(buffer, 0, read);
			}
			byte[] md5sum = digest.digest();
			BigInteger bigInt = new BigInteger(1, md5sum);
			String output = bigInt.toString(16);
			// Fill to 32 chars
			output = String.format("%32s", output).replace(' ', '0');
			return output;
		} catch (IOException e) {
			throw new RuntimeException("Unable to process file for MD5", e);
		} finally {
			try {
				is.close();
			} catch (IOException e) {
				Log.e("dtp_net", "Exception on closing MD5 input stream", e);
			}
		}
	}


	/* returns true when succeeds */
	private boolean SendRequestToDProx(int socket, String url, long file_len,
			int deadline) {
		
		if (url.startsWith("http://"))
			url = url.substring(7);
		int at = url.indexOf("/");
		String q = q1 + url.substring(at) + q2 + url.substring(0, at) + q3;
		if (file_len > 0)
			q = q + RANGE + file_len + "-";
		q = q + "\n\n";

		byte[] buf = q.getBytes();
		if ((dtpwrite(socket, buf, buf.length)) == -1) {
			return false;
		}

		return true;
	}

	/* returns response code (i.e. 200) on success, -1 on failure */
	private int parseStatusLine(String header) {
		int code;
		String statusLine = header.substring(0, header.indexOf("\n"));

		/* check the status line (i.e. HTTP/1.0 200) */
		if (statusLine.indexOf(HTTP_1) != 0)
			return -1;

		char n = statusLine.charAt(HTTP_1.length());
		/* accept only HTTP/1.0 and HTTP/1.1 */
		if (n != '0' && n != '1')
			return -1;

		int p = HTTP_1.length() + 1;
		while (statusLine.charAt(p) == ' ')
			p++;

		try {
			code = Integer.parseInt(statusLine.substring(p, p + 3));
		} catch (NumberFormatException e) {
			return -1;
		}

		return code;

	}

	/* returns parsed value on success, null on failure */
	private String parseField(String header, String field) {
		int pick, pick2;
		if ((pick = header.indexOf(field)) == -1)
			return null;

		pick += field.length();
		pick2 = header.indexOf("\n", pick) - 1;
		if (pick < 0 || pick2 > header.length() || pick > pick2)
			return null;

		return header.substring(pick, pick2);
	}

	/* Wrappers for C native functions */
	private static byte[] intToByteArray(final int integer) {
		ByteBuffer buff = ByteBuffer.allocate(Integer.SIZE / 8);
		buff.putInt(integer);
		buff.order(ByteOrder.BIG_ENDIAN);
		return buff.array();
	}
	
	private void CloseDTPNetThread() {
		//m_wlWifiLock.release();
		//m_wlWakeLock.release();
	}
	
	public void SendLogToServer() {
	}

	public native int dtpsocket();

	public native int dtpconnect(int socket, String strip, int port);

	public native int dtpwrite(int socket, byte[] bs, int size);

	public native int dtpread(int socket, byte[] bs, int size);

	public native int dtpclose(int socket);

	public native int dtpsetsockopt(int socket, int level, int optname,
			int optval);

	public native int dtpgetsockopt(int socket, int level, int optname,
			int optval);

	public native int dtpselect(int maxfd, byte[] readfd, int timeout);
	
	public static native int dtpgetlocktime();

	public static final String q1 = "GET ";
	public static final String q2 = " HTTP/1.1\nHost: ";
	public static final String q3 = ":80\nConnection: close";
	public static final String RANGE = "\nRange: bytes=";

	public static final String HTTP_1 = "HTTP/1.";
	public static final String HTTP_CONT_LENGTH = "Content-Length: ";
	public static final String HTTP_CONT_TYPE = "Content-Type: ";
	public static final String HTTP_CONT_RANGE = "Content-Range: bytes */";
	public static final String HTTP_CONT_BYTES = "Content-Range: bytes ";
	public static final String HTTP_LOCATION = "Location: ";

	public static final int DTP_SO_ACCEPT_CONN = 0;
	public static final int DTP_SO_RCVBUF = 1;
	public static final int DTP_SO_SNDBUF = 2;
	public static final int DTP_SO_KEEPALIVE = 3;
	public static final int DTP_KEEPIDLE = 4;
	public static final int DTP_KEEPINTVL = 5;
	public static final int DTP_KEEPCNT = 6;
	public static final int DTP_SO_LINGER = 7;
	public static final int DTP_SO_BLOCKSIZE = 8;
	public static final int DTP_SO_DEADLINE = 9;
	public static final int DTP_SO_WIFIONLY = 10;
	public static final int DTP_SO_UPBYTE = 11;
	public static final int DTP_SO_DOWNBYTE = 12;
	public static final int DTP_SO_MOBILETIME = 13;
	public static final int DTP_SO_WIFITIME = 14;	
	public static final int DTP_SO_FLOWID = 15;
	public static long unsigned32(int n) {
		return n & 0xFFFFFFFFL;
	}
	public String printAppLogging(DTPNetFlow flow, int status) {
		if (flow == null || flow.socket == 0)
			return null;

		SimpleDateFormat printFormat;
		printFormat = new SimpleDateFormat ("yyyy-MM-dd(EEE) HH:mm:ss", Locale.US);

		String s = "";
		s = s + flow.uuid + " ";
		s = s + unsigned32(dtpgetsockopt(flow.socket, 0, DTP_SO_FLOWID, 0)) + " ";
		s = s + "[" + printFormat.format(flow.startTime) + "] ";
		s = s + flow.url + " ";
		s = s + status + " ";
		s = s + flow.partialContentIndex + " ";
		s = s + dtpgetsockopt(flow.socket, 0, DTP_SO_BLOCKSIZE, 0) + " ";
		s = s + flow.down_len + " ";
		s = s + (dtpgetsockopt(flow.socket, 0, DTP_SO_DEADLINE, 0)) + " ";
		s = s + String.format("%.3f", (System.currentTimeMillis()/1000.0 - (flow.startTime / 1000.0))) + "\n";

		Log.d("dtn_log", s);
		return s;
	}
	static {
		System.loadLibrary("dtn_manager");
	}
}