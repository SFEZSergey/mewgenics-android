package org.anyps5.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.util.Locale;

// Редактор сенсорного управления: элементы перетаскиваются пальцем, панель в центре меняет размер
// выбранного (или всех), прячет его, добавляет/убирает кнопки (правый стик, R3…), задаёт прозрачность
// и сбрасывает раскладку. Всё сохраняется сразу (SharedPreferences "touch_controls").
public final class TouchLayoutActivity extends Activity {
    private TouchControlsView controls;
    private TextView hint;
    private Button opacityButton;

    private boolean russian() {
        final String chosen = getSharedPreferences("MainActivity", MODE_PRIVATE).getString("ui_language", "system");
        if (chosen.equals("ru")) return true;
        if (chosen.equals("en")) return false;
        return Locale.getDefault().getLanguage().equals("ru");
    }
    private String t(String ru, String en) { return russian() ? ru : en; }

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams attributes = getWindow().getAttributes();
            attributes.layoutInDisplayCutoutMode = Build.VERSION.SDK_INT >= 30 ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(attributes);
        }
        hideSystemBars();
        final float dp = getResources().getDisplayMetrics().density;
        FrameLayout root = new FrameLayout(this);
        root.setBackground(new android.graphics.drawable.GradientDrawable(android.graphics.drawable.GradientDrawable.Orientation.TL_BR, new int[] {0xff2b4a2e, 0xff142818}));
        controls = new TouchControlsView(this, true);
        root.addView(controls, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setGravity(Gravity.CENTER_HORIZONTAL);
        hint = new TextView(this);
        hint.setTextColor(0xffdcedc8); hint.setTextSize(13); hint.setGravity(Gravity.CENTER);
        hint.setPadding(0, 0, 0, (int)(6*dp));
        panel.addView(hint);
        LinearLayout row1 = new LinearLayout(this); row1.setGravity(Gravity.CENTER);
        LinearLayout row2 = new LinearLayout(this); row2.setGravity(Gravity.CENTER); row2.setPadding(0, (int)(6*dp), 0, 0);
        addButton(row1, t("Меньше", "Smaller"), new View.OnClickListener() { @Override public void onClick(View v) { controls.scaleSelected(1f / 1.1f); } });
        addButton(row1, t("Больше", "Bigger"), new View.OnClickListener() { @Override public void onClick(View v) { controls.scaleSelected(1.1f); } });
        addButton(row1, t("Скрыть", "Hide"), new View.OnClickListener() { @Override public void onClick(View v) {
            if (controls.selected() != null) controls.setControlOn(controls.selected(), false);
        } });
        addButton(row1, t("Кнопки…", "Buttons…"), new View.OnClickListener() { @Override public void onClick(View v) { chooseButtons(); } });
        opacityButton = addButton(row2, "", new View.OnClickListener() { @Override public void onClick(View v) {
            final float[] steps = {1f, 0.75f, 0.5f, 0.3f};
            int next = 0;
            for (int i = 0; i < steps.length; i++) if (Math.abs(steps[i] - controls.opacity()) < 0.01f) next = (i + 1) % steps.length;
            controls.setOpacity(steps[next]);
            updateOpacityLabel();
        } });
        addButton(row2, t("Сбросить", "Reset"), new View.OnClickListener() { @Override public void onClick(View v) {
            new AlertDialog.Builder(TouchLayoutActivity.this)
                .setMessage(t("Вернуть раскладку по умолчанию?", "Restore the default layout?"))
                .setPositiveButton(t("Сбросить", "Reset"), new DialogInterface.OnClickListener() { @Override public void onClick(DialogInterface d, int w) { controls.resetLayout(); updateOpacityLabel(); } })
                .setNegativeButton(t("Отмена", "Cancel"), null).show();
        } });
        addButton(row2, t("Готово", "Done"), new View.OnClickListener() { @Override public void onClick(View v) { finish(); } });
        panel.addView(row1);
        panel.addView(row2);
        FrameLayout.LayoutParams panelSize = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER);
        root.addView(panel, panelSize);
        setContentView(root);

        controls.setSelectionListener(new TouchControlsView.SelectionListener() { @Override public void onSelectionChanged(TouchControlsView.Control selected) { updateHint(selected); } });
        updateHint(null);
        updateOpacityLabel();
    }

    private Button addButton(LinearLayout row, String label, View.OnClickListener listener) {
        final float dp = getResources().getDisplayMetrics().density;
        Button button = new Button(this);
        android.graphics.drawable.GradientDrawable shape = new android.graphics.drawable.GradientDrawable();
        shape.setColor(0xcc2e5e32); shape.setCornerRadius(12 * dp);
        button.setBackground(shape); button.setTextColor(0xffffffff); button.setTextSize(13); button.setAllCaps(false);
        button.setPadding((int)(16*dp), (int)(6*dp), (int)(16*dp), (int)(6*dp));
        button.setText(label);
        button.setOnClickListener(listener);
        LinearLayout.LayoutParams size = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        size.leftMargin = (int)(4*dp); size.rightMargin = (int)(4*dp);
        row.addView(button, size);
        return button;
    }

    private void updateHint(TouchControlsView.Control selected) {
        if (selected == null) hint.setText(t("Перетаскивайте элементы пальцем. Без выбора размер меняется у всех.", "Drag controls with your finger. With nothing selected, size changes apply to all."));
        else hint.setText(t("Выбрано: ", "Selected: ") + name(selected));
    }

    private void updateOpacityLabel() {
        opacityButton.setText(t("Непрозрачность: ", "Opacity: ") + Math.round(controls.opacity() * 100) + "%");
    }

    private String name(TouchControlsView.Control c) {
        switch (c.id) {
            case "toggle": return t("Кнопка-глаз (скрыть управление)", "Eye button (hide controls)");
            case "lstick": return t("Левый стик", "Left stick");
            case "rstick": return t("Правый стик", "Right stick");
            case "dpad": return t("Крестовина", "D-pad");
            case "options": return "Options";
            case "touchpad": return t("Сенсорная панель", "Touchpad");
            default: return c.label;
        }
    }

    private void chooseButtons() {
        final java.util.List<TouchControlsView.Control> items = new java.util.ArrayList<>();
        for (TouchControlsView.Control c : controls.controls()) if (c.kind != TouchControlsView.KIND_TOGGLE) items.add(c);
        final String[] names = new String[items.size()];
        final boolean[] checked = new boolean[items.size()];
        for (int i = 0; i < items.size(); i++) { names[i] = name(items.get(i)); checked[i] = items.get(i).on; }
        new AlertDialog.Builder(this)
            .setTitle(t("Элементы управления", "Controls"))
            .setMultiChoiceItems(names, checked, new DialogInterface.OnMultiChoiceClickListener() {
                @Override public void onClick(DialogInterface dialog, int which, boolean on) { controls.setControlOn(items.get(which), on); }
            })
            .setPositiveButton("OK", null)
            .show();
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemBars();
    }

    private void hideSystemBars() {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }
}
