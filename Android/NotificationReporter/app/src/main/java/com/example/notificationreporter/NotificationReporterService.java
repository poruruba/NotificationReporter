package com.example.notificationreporter;

import android.app.Notification;
import android.content.pm.ApplicationInfo;
import android.database.sqlite.SQLiteDatabase;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.util.Log;
import org.eclipse.paho.client.mqttv3.DisconnectedBufferOptions;
import org.eclipse.paho.client.mqttv3.IMqttActionListener;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.IMqttToken;
import org.eclipse.paho.client.mqttv3.MqttAsyncClient;
import org.eclipse.paho.client.mqttv3.MqttCallbackExtended;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import org.json.JSONObject;

public class NotificationReporterService extends NotificationListenerService {
    MqttAsyncClient mqttClient;
    NotificationDbHelper helper;
    SQLiteDatabase db;
    String serverUri = "tcp://【MQTTブローカのホスト名】:【MQTTブローカのポート番号】";
    String topic = "android/notify"; // MQTTトピック名
    final static int ICON_SIZE = 32;
    final static String clientId = "TestClient"; // MQTT接続時のクライアント名

    @Override
    public void onCreate() {
        super.onCreate();

        Log.d(MainActivity.TAG, "NotificationMonitorService onCreate");
        helper = new NotificationDbHelper(this);
        db = helper.getReadableDatabase();

        try {
            mqttClient = new MqttAsyncClient(serverUri, clientId, new MemoryPersistence());
            mqttClient.setCallback(new MqttCallbackExtended() {
                @Override
                public void connectComplete(boolean reconnect, String serverURI) {
                    if (reconnect) {
                        Log.d(MainActivity.TAG, "Reconnected to : " + serverURI);
                    } else {
                        Log.d(MainActivity.TAG, "Connected to: " + serverURI);
                    }

                    try{
                        JSONObject message_json = new JSONObject();
                        message_json.put("id", -1);
                        mqttClient.publish(topic, message_json.toString().getBytes(), 0, false);
                    }catch(Exception ex){
                        Log.e(MainActivity.TAG, ex.getMessage());
                    }
                }

                @Override
                public void connectionLost(Throwable cause) {
                    Log.e(MainActivity.TAG, "The Connection was lost.");
                }

                @Override
                public void messageArrived(String topic, MqttMessage message) throws Exception {
                    Log.d(MainActivity.TAG, "Incoming message: " + new String(message.getPayload()));
                }

                @Override
                public void deliveryComplete(IMqttDeliveryToken token) {
                    Log.d(MainActivity.TAG, "deliveryComplete");
                }
            });

            MqttConnectOptions options = new MqttConnectOptions();
            options.setCleanSession(true);
            options.setAutomaticReconnect(true);
            mqttClient.connect(options, null, new IMqttActionListener() {
                @Override
                public void onSuccess(IMqttToken asyncActionToken) {
                    Log.d(MainActivity.TAG, "onSuccess");
                }

                @Override
                public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                    Log.e(MainActivity.TAG, "Failed to connect to: " + serverUri);
                }
            });
        }catch(Exception ex){
            Log.e(MainActivity.TAG, ex.getMessage());
        }
    }

    @Override
    public void onNotificationPosted(StatusBarNotification sbn) {
        Log.d(MainActivity.TAG,"onNotificationPosted");
        processNotification(sbn, true);
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification sbn) {
        Log.d(MainActivity.TAG,"onNotificationRemoved");
        processNotification(sbn, false);
    }

    private void processNotification( StatusBarNotification sbn, boolean posted ){
        int id = sbn.getId();
        String packageName = sbn.getPackageName();
        String groupKey = sbn.getGroupKey();
        String key = sbn.getKey();
        String tag = sbn.getTag();
        long time = sbn.getPostTime();

        Log.d(MainActivity.TAG,"id:" + id + " packageName:" + packageName + " posted:" + posted + " time:" +time);
        Log.d(MainActivity.TAG,"groupKey:" + groupKey + " key:" + key + " tag:" + tag);

        try {
            ApplicationInfo app = MainActivity.packageManager.getApplicationInfo(packageName, 0);
            String label = app.loadLabel(MainActivity.packageManager).toString();

            Notification notification = sbn.getNotification();
            CharSequence tickerText = notification.tickerText;
            Bundle extras = notification.extras;
            String title = extras.getString(Notification.EXTRA_TITLE);
            CharSequence text = extras.getCharSequence(Notification.EXTRA_TEXT);
            CharSequence subText = extras.getCharSequence(Notification.EXTRA_SUB_TEXT);
            CharSequence infoText = extras.getCharSequence(Notification.EXTRA_INFO_TEXT);
            Log.d(MainActivity.TAG, "Title:" + title + " Text:" + text + " subText:" + subText + " infoText:" + infoText + "tickerText:" + tickerText);

            Icon smallIcon = notification.getSmallIcon();
            Drawable icon = smallIcon.loadDrawable(this);
            Log.d(MainActivity.TAG, "width:" + icon.getIntrinsicWidth() + " height:" + icon.getIntrinsicHeight());
            Bitmap bitmap = Bitmap.createBitmap(ICON_SIZE, ICON_SIZE, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            icon.setBounds(0, 0, ICON_SIZE, ICON_SIZE);
            icon.draw(canvas);

            byte[] bmp_bin = new byte[ICON_SIZE * ((ICON_SIZE + 7) / 8)];
            for( int y = 0 ; y < ICON_SIZE ; y++ ){
                for( int x = 0 ; x < ICON_SIZE ; x++ ){
                    Color color = bitmap.getColor(x, y);
                    float alpha = color.alpha();
                    double gray = 0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue();
                    if( gray >= 0.5 )
                        bmp_bin[y * ((ICON_SIZE + 7) / 8) + x / 8] |= 0x0001 << ( 8 - (x % 8) - 1);
                }
            }
            bitmap.recycle();

            if( helper.hasPackageName(db, packageName ) ) {
                JSONObject message_json = new JSONObject();
                message_json.put("name", packageName);
                message_json.put("posted", posted);
                message_json.put("id", id);
                message_json.put("icon", Bin2Hex(bmp_bin));
                message_json.put("time", time);
                if (title != null) message_json.put("title", title);
                if (subText != null) message_json.put("subtext", subText);
                if( label != null ) message_json.put("label", label);

                mqttClient.publish(topic, message_json.toString().getBytes(), 0, false);
            }
        }catch(Exception ex){
            Log.e(MainActivity.TAG, ex.getMessage());
        }
    }

    public static String Bin2Hex(byte[] array){
        StringBuilder sb = new StringBuilder();
        for (byte d: array) {
            sb.append(String.format("%02X", d));
        }
        return sb.toString();
    }

    @Override
    public void onDestroy() {
        Log.d(MainActivity.TAG, "onDestroy");

        try {
            mqttClient.disconnect();
            mqttClient.close();
        }catch(Exception ex){
            Log.e(MainActivity.TAG, ex.getMessage());
        }
    }
}
