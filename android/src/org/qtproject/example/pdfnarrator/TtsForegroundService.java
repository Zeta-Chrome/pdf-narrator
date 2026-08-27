package org.qtproject.example.pdfnarrator;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;

public class TtsForegroundService extends Service {
    private static final String CHANNEL_ID = "tts_playback_channel";
    private static final int NOTIF_ID = 1001;
    private PowerManager.WakeLock wakeLock;

    public static void start(Context ctx, String title, String subtitle) {
        Intent i = new Intent(ctx, TtsForegroundService.class);
        i.putExtra("title", title);
        i.putExtra("subtitle", subtitle);
        ctx.startForegroundService(i);
    }

    public static void stop(Context ctx) {
        ctx.stopService(new Intent(ctx, TtsForegroundService.class));
    }

    @Override public void onCreate() {
        super.onCreate();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationManager nm = getSystemService(NotificationManager.class);
            nm.createNotificationChannel(new NotificationChannel(
                CHANNEL_ID, "Playback", NotificationManager.IMPORTANCE_LOW));
        }
        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "TtsReader:PlaybackWakeLock");
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        String title = intent != null ? intent.getStringExtra("title") : "Reading";
        String subtitle = intent != null ? intent.getStringExtra("subtitle") : "";

        Notification.Builder b = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
            ? new Notification.Builder(this, CHANNEL_ID) : new Notification.Builder(this);
        Notification n = b.setContentTitle(title).setContentText(subtitle)
            .setSmallIcon(android.R.drawable.ic_media_play) // swap for your app icon
            .setOngoing(true).build();

        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(NOTIF_ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
        } else {
            startForeground(NOTIF_ID, n);
        }

        if (!wakeLock.isHeld()) wakeLock.acquire(10 * 60 * 60 * 1000L); // 10hr safety cap
        return START_STICKY;
    }

    @Override public void onDestroy() {
        if (wakeLock.isHeld()) wakeLock.release();
        super.onDestroy();
    }

    @Override public IBinder onBind(Intent intent) { return null; }
}
