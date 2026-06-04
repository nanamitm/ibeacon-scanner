package io.github.nanamitm.ibeaconscanner;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.util.Base64;
import android.util.Log;
import java.util.concurrent.ConcurrentLinkedQueue;

/**
 * C++ 側 JNI コールバック方式の代わりに、Java がキューにスキャン結果を蓄積し
 * C++ 側が poll() で定期的に取り出すポーリング方式を採用する。
 * JNI ネイティブメソッド登録が不要なため信頼性が高い。
 */
public class QtBeaconScanner {

    private static final String TAG = "QtBeaconScanner";

    private BluetoothLeScanner mScanner;
    private ScanCallback       mCallback;
    private boolean            mScanning = false;

    // スキャン結果キュー: "address\nname\nrssi\nscanRecordBase64" 形式
    private final ConcurrentLinkedQueue<String> mQueue = new ConcurrentLinkedQueue<>();

    public boolean startScan(Context context) {
        BluetoothManager mgr =
            (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        if (mgr == null) { Log.e(TAG, "BluetoothManager null"); return false; }

        BluetoothAdapter adapter = mgr.getAdapter();
        if (adapter == null || !adapter.isEnabled()) { Log.e(TAG, "Adapter not ready"); return false; }

        mScanner = adapter.getBluetoothLeScanner();
        if (mScanner == null) { Log.e(TAG, "Scanner null"); return false; }

        ScanSettings settings = new ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .setReportDelay(0)
            .build();

        mCallback = new ScanCallback() {
            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                try {
                    String address = "";
                    try { address = result.getDevice().getAddress(); } catch (Exception ignored) {}

                    String name = "";
                    try {
                        String n = result.getDevice().getName();
                        if (n != null) name = n;
                    } catch (Exception ignored) {}

                    int rssi = result.getRssi();

                    String scanB64 = "";
                    ScanRecord rec = result.getScanRecord();
                    if (rec != null && rec.getBytes() != null) {
                        scanB64 = Base64.encodeToString(rec.getBytes(), Base64.NO_WRAP);
                    }

                    mQueue.offer(address + "\n" + name + "\n" + rssi + "\n" + scanB64);
                    Log.d(TAG, "queued: " + address + " rssi=" + rssi);
                } catch (Exception e) {
                    Log.e(TAG, "onScanResult error: " + e.getMessage());
                }
            }

            @Override
            public void onBatchScanResults(java.util.List<ScanResult> results) {
                for (ScanResult r : results) onScanResult(0, r);
            }

            @Override
            public void onScanFailed(int errorCode) {
                Log.e(TAG, "Scan failed: " + errorCode);
                mQueue.offer("\nSCAN_FAILED\n" + errorCode + "\n");
            }
        };

        try {
            mScanner.startScan(null, settings, mCallback);
            mScanning = true;
            Log.d(TAG, "startScan OK");
            return true;
        } catch (Exception e) {
            Log.e(TAG, "startScan exception: " + e.getMessage());
            return false;
        }
    }

    public void stopScan() {
        if (mScanner != null && mCallback != null && mScanning) {
            try { mScanner.stopScan(mCallback); } catch (Exception e) { Log.e(TAG, e.getMessage()); }
        }
        mScanning = false;
        mCallback = null;
        mQueue.clear();
    }

    /** C++ 側から定期的に呼び出してキューから1件取得する。結果がなければ null を返す。 */
    public String poll() {
        return mQueue.poll();
    }

    /** キューの現在サイズ（診断用） */
    public int queueSize() {
        return mQueue.size();
    }
}
