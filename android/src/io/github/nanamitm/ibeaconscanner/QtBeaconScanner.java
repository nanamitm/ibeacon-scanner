package io.github.nanamitm.ibeaconscanner;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.util.Log;

/**
 * Qt の QBluetoothDeviceDiscoveryAgent は non-connectable BLE 広告
 * (iBeacon 等) を Android で検出できないため、BluetoothLeScanner を直接使用する。
 */
public class QtBeaconScanner {

    private static final String TAG = "QtBeaconScanner";

    private BluetoothLeScanner mScanner;
    private ScanCallback       mCallback;
    private boolean            mScanning = false;

    // C++ 側のコールバック (JNI から登録)
    private long mNativePtr = 0;

    public QtBeaconScanner(long nativePtr) {
        mNativePtr = nativePtr;
    }

    public boolean startScan(Context context) {
        BluetoothManager mgr =
            (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        if (mgr == null) return false;

        BluetoothAdapter adapter = mgr.getAdapter();
        if (adapter == null || !adapter.isEnabled()) return false;

        mScanner = adapter.getBluetoothLeScanner();
        if (mScanner == null) return false;

        ScanSettings settings = new ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)  // 常時スキャン
            .setReportDelay(0)                                 // 即時報告
            .build();

        mCallback = new ScanCallback() {
            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                processScanResult(result);
            }

            @Override
            public void onBatchScanResults(java.util.List<ScanResult> results) {
                for (ScanResult r : results) processScanResult(r);
            }

            @Override
            public void onScanFailed(int errorCode) {
                Log.e(TAG, "Scan failed: " + errorCode);
                nativeOnScanFailed(mNativePtr, errorCode);
            }
        };

        try {
            mScanner.startScan(null, settings, mCallback);
            mScanning = true;
            return true;
        } catch (Exception e) {
            Log.e(TAG, "startScan exception: " + e.getMessage());
            return false;
        }
    }

    public void stopScan() {
        if (mScanner != null && mCallback != null && mScanning) {
            try {
                mScanner.stopScan(mCallback);
            } catch (Exception e) {
                Log.e(TAG, "stopScan exception: " + e.getMessage());
            }
        }
        mScanning = false;
        mCallback = null;
    }

    private void processScanResult(ScanResult result) {
        String address = "";
        try { address = result.getDevice().getAddress(); } catch (Exception ignored) {}

        String name = "";
        try { name = result.getDevice().getName(); } catch (Exception ignored) {}
        if (name == null) name = "";

        int rssi = result.getRssi();

        byte[] scanRecord = new byte[0];
        ScanRecord rec = result.getScanRecord();
        if (rec != null && rec.getBytes() != null) {
            scanRecord = rec.getBytes();
        }

        nativeOnScanResult(mNativePtr, address, name, rssi, scanRecord);
    }

    // C++ 側のネイティブメソッド
    private native void nativeOnScanResult(long nativePtr,
                                           String address,
                                           String name,
                                           int rssi,
                                           byte[] scanRecord);
    private native void nativeOnScanFailed(long nativePtr, int errorCode);
}
