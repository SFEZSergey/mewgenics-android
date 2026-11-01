package org.anyps5.android;

import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import org.libsdl.app.SDLActivity;

public final class GameActivity extends SDLActivity {
    // Состояние виртуального геймпада (TouchControlsView) для libSceVideoOut; реализовано в libmain.
    static native void nativeVirtualPad(int buttons, int leftX, int leftY, int rightX, int rightY);
    // Сенсорная мышь: сдвиг курсора и кнопка (1 — левая, 2 — правая; 0 — только сдвиг).
    static native void nativeTouchMouse(int dx, int dy, int button, boolean pressed);
    // Прямой тач: абсолютные координаты [0..1920, 0..1080] и кнопка (1 — левая, 2 — правая; 0 — только позиция).
    static native void nativeTouchDirect(int x, int y, int button, boolean pressed);
    // Ввод текста для игры (sceImeDialog): запрос (начальный текст или null), предел длины, ещё нужен ли
    // показанный диалог, ответ (null — отмена). Реализовано в libmain.
    static native String nativeTextInputRequest();
    static native int nativeTextInputMaxLength();
    static native boolean nativeTextInputPending();
    static native void nativeTextInputDone(String text);
    // Сохранить кэш трансляции box64 (в фоне, отдельным потоком libmain); реализовано в libmain.
    static native void nativeSaveDynacache();

    private final android.os.Handler textPoll = new android.os.Handler(android.os.Looper.getMainLooper());
    private android.app.AlertDialog textDialog;
    private final Runnable pollText = new Runnable() { @Override public void run() {
        try {
            if (textDialog != null && !nativeTextInputPending()) {
                // Игра прервала ввод.
                textDialog.dismiss();
                textDialog = null;
            }
            if (textDialog == null) {
                final String initial = nativeTextInputRequest();
                if (initial != null) showTextInput(initial, nativeTextInputMaxLength());
            }
        } catch (UnsatisfiedLinkError ignored) {
            return;
        }
        textPoll.postDelayed(this, 200);
    } };

    @Override protected String[] getLibraries() {
        return new String[] { "c++_shared", "SDL2", "main" };
    }
    @Override protected String[] getArguments() {
        java.io.File game = new java.io.File(getFilesDir(), "game");
        java.io.File executable = new java.io.File(game, "app.elf");
        return new String[] { executable.getAbsolutePath(), getApplicationInfo().nativeLibraryDir, getFilesDir().getAbsolutePath() };
    }

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        // Рисуем и под вырезом камеры: игра занимает весь экран.
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams attributes = getWindow().getAttributes();
            attributes.layoutInDisplayCutoutMode = Build.VERSION.SDK_INT >= 30 ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(attributes);
        }
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        // Тестовый прогон автопилота (tools/autopilot.sh кладёт сценарий): игра включает экран и
        // показывается поверх экрана блокировки, пока идёт прогон.
        if (new java.io.File(new java.io.File(getFilesDir(), "game"), "autopilot.txt").isFile() && Build.VERSION.SDK_INT >= 27) {
            setShowWhenLocked(true);
            setTurnScreenOn(true);
        }
        hideSystemBars();
        // Сенсорное управление поверх поверхности SDL (касания мимо элементов уходят в игру).
        if (mLayout != null) {
            mLayout.setMotionEventSplittingEnabled(true);
            mLayout.addView(new TouchControlsView(this), new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        }
    }

    // С root (настройка «Не выгружать Dolby Atmos»): пока процесс игры жив, процессы Dolby получают
    // oom_score_adj -800, чтобы система при нехватке памяти выгружала другие фоновые приложения.
    // Система пересчитывает приоритет при смене состояния процессов, поэтому раз в 5 с повторяется.
    private static boolean backgroundProtected = false;
    private void protectBackground() {
        if (backgroundProtected || !getSharedPreferences("MainActivity", MODE_PRIVATE).getBoolean("protect_dolby", false)) return;
        backgroundProtected = true;
        final int self = android.os.Process.myPid();
        new Thread(new Runnable() { @Override public void run() {
            try {
                Process su = Runtime.getRuntime().exec("su");
                java.io.OutputStream in = su.getOutputStream();
                in.write(("while [ -d /proc/" + self + " ]; do for p in $(ps -A -o PID,NAME | grep -i dolby | awk '{print $1}'); do echo -800 > /proc/$p/oom_score_adj; done; sleep 5; done; exit\n").getBytes());
                in.flush();
                in.close();
            } catch (Exception e) {
                backgroundProtected = false;
            }
        } }).start();
    }

    @Override protected void onResume() {
        super.onResume();
        protectBackground();
        textPoll.removeCallbacks(pollText);
        textPoll.postDelayed(pollText, 500);
    }

    @Override protected void onPause() {
        textPoll.removeCallbacks(pollText);
        try { nativeSaveDynacache(); } catch (UnsatisfiedLinkError ignored) {}
        super.onPause();
    }

    private boolean russian() {
        final String chosen = getSharedPreferences("MainActivity", MODE_PRIVATE).getString("ui_language", "en");
        if (chosen.equals("ru")) return true;
        if (chosen.equals("en")) return false;
        return java.util.Locale.getDefault().getLanguage().equals("ru");
    }

    // Поле ввода с клавиатурой телефона; «Готово» на клавиатуре или OK отдают текст игре.
    private void showTextInput(String initial, int maxLength) {
        final boolean ru = russian();
        final android.widget.EditText field = new android.widget.EditText(this);
        field.setSingleLine(true);
        field.setFilters(new android.text.InputFilter[] { new android.text.InputFilter.LengthFilter(Math.max(1, maxLength)) });
        field.setText(initial);
        field.setSelection(field.getText().length());
        field.setImeOptions(android.view.inputmethod.EditorInfo.IME_ACTION_DONE | android.view.inputmethod.EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        final float dp = getResources().getDisplayMetrics().density;
        android.widget.FrameLayout box = new android.widget.FrameLayout(this);
        box.setPadding((int)(20*dp), (int)(8*dp), (int)(20*dp), 0);
        box.addView(field);
        final boolean[] answered = {false};
        textDialog = new android.app.AlertDialog.Builder(this)
            .setTitle(ru ? "Ввод текста" : "Enter text")
            .setView(box)
            .setPositiveButton("OK", new android.content.DialogInterface.OnClickListener() { @Override public void onClick(android.content.DialogInterface d, int w) {
                answered[0] = true;
                nativeTextInputDone(field.getText().toString());
            } })
            .setNegativeButton(ru ? "Отмена" : "Cancel", new android.content.DialogInterface.OnClickListener() { @Override public void onClick(android.content.DialogInterface d, int w) {
                answered[0] = true;
                nativeTextInputDone(null);
            } })
            .create();
        textDialog.setOnDismissListener(new android.content.DialogInterface.OnDismissListener() { @Override public void onDismiss(android.content.DialogInterface d) {
            if (!answered[0]) { answered[0] = true; nativeTextInputDone(null); }
            textDialog = null;
            hideSystemBars();
        } });
        field.setOnEditorActionListener(new android.widget.TextView.OnEditorActionListener() { @Override public boolean onEditorAction(android.widget.TextView v, int action, android.view.KeyEvent e) {
            if (action != android.view.inputmethod.EditorInfo.IME_ACTION_DONE && e == null) return false;
            answered[0] = true;
            nativeTextInputDone(field.getText().toString());
            if (textDialog != null) textDialog.dismiss();
            return true;
        } });
        textDialog.getWindow().setSoftInputMode(android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
        textDialog.show();
        field.requestFocus();
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemBars();
    }

    // Полноэкранный immersive-режим: строка состояния и навигационная полоса скрыты, появляются
    // временно по свайпу от края.
    private void hideSystemBars() {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }
}
