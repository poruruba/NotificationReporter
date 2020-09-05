package com.example.notificationreporter;

import androidx.appcompat.app.AppCompatActivity;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.database.sqlite.SQLiteDatabase;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;
import java.text.Collator;
import java.util.Collections;
import java.util.Comparator;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class MainActivity extends AppCompatActivity{
    public static final String TAG = "NotifyMon";
    public static PackageManager packageManager;
    PackageListAdapter adapter;
    NotificationDbHelper helper;
    SQLiteDatabase db;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        helper = new NotificationDbHelper(this);
        db = helper.getWritableDatabase();

        packageManager = getPackageManager();
        List<ApplicationInfo> dataList = packageManager.getInstalledApplications(PackageManager.GET_META_DATA);

        ListView list;
        list = (ListView)findViewById(R.id.list_app);
        adapter = new PackageListAdapter(dataList);
        list.setAdapter(adapter);
    }

    private class PackageListAdapter extends BaseAdapter {
        List<ApplicationInfo> dataList;

        public PackageListAdapter(List<ApplicationInfo> dataList){
            this.dataList = new ArrayList<>();

            for (ApplicationInfo info : dataList) {
                if ((info.flags & ApplicationInfo.FLAG_SYSTEM) == ApplicationInfo.FLAG_SYSTEM)
                    continue;

                this.dataList.add(info);
            }

            Collections.sort(this.dataList, new Comparator<ApplicationInfo>(){
                Collator collector = Collator.getInstance(Locale.JAPANESE);
                @Override
                public int compare(ApplicationInfo p1, ApplicationInfo p2) {
                    return collector.compare(p1.loadLabel(packageManager).toString(), p2.loadLabel(packageManager).toString());
                }
            });
        }

        @Override
        public int getCount() {
            return dataList.size();
        }

        @Override
        public Object getItem(int position) {
            return dataList.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView( final int position, View convertView, ViewGroup parent) {
            if(convertView == null){
                LayoutInflater inflater = (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
                convertView = inflater.inflate(R.layout.package_list, null);
            }

            ApplicationInfo aInfo = (ApplicationInfo)getItem(position);
            if(aInfo != null){
                String packageName = aInfo.packageName;
                String label = aInfo.loadLabel(packageManager).toString();
                Drawable icon = aInfo.loadIcon(packageManager);
                Log.d(MainActivity.TAG, "icon_width:" + icon.getIntrinsicWidth() + " icon_height:" + icon.getIntrinsicHeight());

                TextView text;
                ImageView image;
                CheckBox chk;

                text = (TextView) convertView.findViewById(R.id.txt_package_name);
                text.setText(packageName);
                text = (TextView) convertView.findViewById(R.id.txt_label);
                text.setText(label);
                image = (ImageView) convertView.findViewById(R.id.img_icon);
                image.setImageDrawable(icon);
                chk = (CheckBox)convertView.findViewById(R.id.chk_allowed);
                chk.setOnCheckedChangeListener(null);
                chk.setChecked(helper.hasPackageName(db, packageName));
                chk.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
                        ApplicationInfo app = (ApplicationInfo)adapter.getItem(position);
                        String packageName = app.packageName;
                        Log.d(MainActivity.TAG, "checked packageName:" + packageName + " b:" + b);
                        if( b )
                            helper.insertPackageName(db, packageName);
                        else
                            helper.deletePackageName(db, packageName);
                    }
                });
            }
            return convertView;
        }
    }
}
