package org.anyps5.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

// Виртуальный геймпад поверх игры: левый и правый стики, крестовина, ✕○□△, L1/R1/L2/R2, L3/R3, Options,
// сенсорная панель. Состояние уходит в libmain (GameActivity.nativeVirtualPad), libSceVideoOut подмешивает
// его к вводу. Касания мимо элементов проходят в игру. Кнопка-глаз скрывает/показывает слой (запоминается);
// при подключённом физическом геймпаде слой при старте скрыт.
// Раскладка настраивается в TouchLayoutActivity (режим редактирования): положение (доля ширины/высоты),
// размер, включение каждого элемента и общая непрозрачность хранятся в SharedPreferences "touch_controls".
public final class TouchControlsView extends View {
    // Биты кнопок PS5 (libScePad PadButton).
    static final int L3 = 0x0002, R3 = 0x0004, OPTIONS = 0x0008, UP = 0x0010, RIGHT = 0x0020, DOWN = 0x0040, LEFT = 0x0080;
    static final int L2 = 0x0100, R2 = 0x0200, L1 = 0x0400, R1 = 0x0800;
    static final int TRIANGLE = 0x1000, CIRCLE = 0x2000, CROSS = 0x4000, SQUARE = 0x8000, TOUCHPAD = 0x100000;

    static final int KIND_BUTTON = 0, KIND_STICK = 1, KIND_DPAD = 2, KIND_TOGGLE = 3;

    static final class Control {
        final String id;
        final int kind;
        final int bit;
        final String label;
        final float defaultX, defaultY, defaultSize;
        final boolean defaultOn;
        float x, y, scale = 1f;
        boolean on;
        final RectF bounds = new RectF();
        float cx, cy, radius;
        final float[] vector = new float[2];
        Control(String id, int kind, int bit, String label, float x, float y, float size, boolean on) {
            this.id = id; this.kind = kind; this.bit = bit; this.label = label;
            defaultX = x; defaultY = y; defaultSize = size; defaultOn = on;
        }
        boolean hit(float px, float py) {
            if (kind == KIND_DPAD) return bounds.contains(px, py);
            final float dx = px - cx, dy = py - cy, r = radius * 1.25f;
            return dx * dx + dy * dy <= r * r;
        }
    }

    // Элементы по умолчанию: положение — доля ширины/высоты, размер — радиус в % короткой стороны.
    static Control[] createControls() {
        return new Control[] {
            new Control("toggle", KIND_TOGGLE, 0, "◉", 0.50f, 0.06f, 4f, true),
            new Control("lstick", KIND_STICK, 0, "L", 0.13f, 0.70f, 15f, true),
            new Control("rstick", KIND_STICK, 1, "R", 0.70f, 0.72f, 13f, false),
            new Control("dpad", KIND_DPAD, 0, "✚", 0.28f, 0.80f, 11f, true),
            new Control("triangle", KIND_BUTTON, TRIANGLE, "△", 0.86f, 0.54f, 6.5f, true),
            new Control("cross", KIND_BUTTON, CROSS, "✕", 0.86f, 0.78f, 6.5f, true),
            new Control("square", KIND_BUTTON, SQUARE, "□", 0.805f, 0.66f, 6.5f, true),
            new Control("circle", KIND_BUTTON, CIRCLE, "○", 0.915f, 0.66f, 6.5f, true),
            new Control("l1", KIND_BUTTON, L1, "L1", 0.07f, 0.30f, 6f, true),
            new Control("l2", KIND_BUTTON, L2, "L2", 0.07f, 0.13f, 6f, true),
            new Control("r1", KIND_BUTTON, R1, "R1", 0.93f, 0.30f, 6f, true),
            new Control("r2", KIND_BUTTON, R2, "R2", 0.93f, 0.13f, 6f, true),
            new Control("l3", KIND_BUTTON, L3, "L3", 0.13f, 0.43f, 4.5f, true),
            new Control("r3", KIND_BUTTON, R3, "R3", 0.70f, 0.45f, 4.5f, false),
            new Control("options", KIND_BUTTON, OPTIONS, "≡", 0.62f, 0.10f, 5f, true),
            new Control("touchpad", KIND_BUTTON, TOUCHPAD, "▭", 0.38f, 0.10f, 5f, true),
        };
    }

    interface SelectionListener { void onSelectionChanged(Control selected); }

    private final Control[] controls;
    private final Control toggle, leftStick, rightStick;
    private final boolean editing;
    // Какой элемент держит каждый палец (по индексу id указателя).
    private final Control[] held = new Control[16];
    private int dpadBits = 0;
    private int buttons = 0;
    private int sentButtons = -1, sentSticks = -1;
    private boolean visible;
    // Сенсорная мышь (режим тачпада, настройка «mouse»): касание мимо кнопок двигает курсор, тап — левый клик,
    // тап двумя пальцами — правый, удержание и ведение — перетаскивание с зажатой левой кнопкой.
    private final boolean touchMouse;
    private int mousePointer = -1, mouseExtraPointer = -1;
    private float mouseX, mouseY, mouseStartX, mouseStartY, mouseRestX, mouseRestY;
    private long mouseDownTime;
    private boolean mouseMoved, mouseSecondFinger, mouseDragging;
    private static final long MOUSE_TAP_MS = 250, DEFAULT_LONG_PRESS_MS = 1500L;
    private final long longPressMs;
    private float opacity;
    private Control selected;
    private float dragDx, dragDy;
    private SelectionListener selectionListener;
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final SharedPreferences preferences;

    public TouchControlsView(Context context) { this(context, false); }

    public TouchControlsView(Context context, boolean editing) {
        super(context);
        this.editing = editing;
        preferences = context.getSharedPreferences("touch_controls", Context.MODE_PRIVATE);
        touchMouse = true;
        longPressMs = preferences.getLong("long_press_ms", DEFAULT_LONG_PRESS_MS);
        visible = editing || preferences.getBoolean("visible", false);
        controls = createControls();
        toggle = controls[0];
        leftStick = controls[1];
        rightStick = controls[2];
        load();
        stroke.setStyle(Paint.Style.STROKE);
        text.setTextAlign(Paint.Align.CENTER);
        text.setTypeface(Typeface.DEFAULT_BOLD);
        setFocusable(false);
    }

    static boolean hasPhysicalGamepad() {
        for (int id : InputDevice.getDeviceIds()) {
            final InputDevice device = InputDevice.getDevice(id);
            if (device == null || device.isVirtual()) continue;
            final int sources = device.getSources();
            if ((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD || (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) return true;
        }
        return false;
    }

    // ---- Раскладка ----

    private void load() {
        opacity = preferences.getFloat("opacity", 1f);
        for (Control c : controls) {
            c.x = preferences.getFloat(c.id + ".x", c.defaultX);
            c.y = preferences.getFloat(c.id + ".y", c.defaultY);
            c.scale = preferences.getFloat(c.id + ".s", 1f);
            c.on = c == toggle || preferences.getBoolean(c.id + ".on", c.defaultOn);
        }
    }

    private void save(Control c) {
        preferences.edit().putFloat(c.id + ".x", c.x).putFloat(c.id + ".y", c.y).putFloat(c.id + ".s", c.scale).putBoolean(c.id + ".on", c.on).apply();
    }

    Control[] controls() { return controls; }
    Control selected() { return selected; }
    float opacity() { return opacity; }
    void setSelectionListener(SelectionListener listener) { selectionListener = listener; }

    void setOpacity(float value) {
        opacity = Math.max(0.2f, Math.min(1f, value));
        preferences.edit().putFloat("opacity", opacity).apply();
        invalidate();
    }

    // Размер выбранного элемента, без выбора — всех.
    void scaleSelected(float factor) {
        for (Control c : controls) {
            if (selected != null && c != selected) continue;
            c.scale = Math.max(0.5f, Math.min(2.5f, c.scale * factor));
            save(c);
        }
        layoutControls();
        invalidate();
    }

    void setControlOn(Control c, boolean on) {
        if (c == toggle) return;
        c.on = on;
        if (!on && selected == c) select(null);
        save(c);
        invalidate();
    }

    void resetLayout() {
        final SharedPreferences.Editor editor = preferences.edit();
        for (Control c : controls) editor.remove(c.id + ".x").remove(c.id + ".y").remove(c.id + ".s").remove(c.id + ".on");
        editor.remove("opacity").apply();
        load();
        select(null);
        layoutControls();
        invalidate();
    }

    private void select(Control c) {
        selected = c;
        if (selectionListener != null) selectionListener.onSelectionChanged(c);
    }

    private float unit() { return Math.min(getWidth(), getHeight()) / 100f; }

    private void layoutControls() {
        final int w = getWidth(), h = getHeight();
        if (w == 0 || h == 0) return;
        final float u = unit();
        for (Control c : controls) {
            final float r = c.defaultSize * c.scale * u;
            c.radius = r;
            c.cx = Math.max(safe.left + r, Math.min(w - safe.right - r, c.x * w));
            c.cy = Math.max(safe.top + r, Math.min(h - safe.bottom - r, c.y * h));
            c.bounds.set(c.cx - r, c.cy - r, c.cx + r, c.cy + r);
        }
        text.setTextSize(5 * u);
        stroke.setStrokeWidth(Math.max(2f, 0.4f * u));
    }

    @Override protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        layoutControls();
    }

    // Окно заходит под вырез камеры: элементы держатся в его безопасной зоне.
    private final android.graphics.Rect safe = new android.graphics.Rect();
    @Override public android.view.WindowInsets onApplyWindowInsets(android.view.WindowInsets insets) {
        android.view.DisplayCutout cutout = android.os.Build.VERSION.SDK_INT >= 28 ? insets.getDisplayCutout() : null;
        if (cutout == null) safe.setEmpty();
        else safe.set(cutout.getSafeInsetLeft(), cutout.getSafeInsetTop(), cutout.getSafeInsetRight(), cutout.getSafeInsetBottom());
        layoutControls();
        invalidate();
        return super.onApplyWindowInsets(insets);
    }

    // ---- Отрисовка ----

    private int alpha(int color) {
        final int a = Math.round(((color >>> 24) & 0xff) * opacity);
        return (a << 24) | (color & 0xffffff);
    }

    @Override protected void onDraw(Canvas canvas) {
        drawCircle(canvas, toggle, visible ? 0x55ffffff : 0x33ffffff, true);
        if (!visible) return;
        for (Control c : controls) {
            if (c == toggle || !c.on) continue;
            if (c.kind == KIND_STICK) {
                drawCircle(canvas, c, 0x22ffffff, editing);
                final float kx = c.cx + c.vector[0] * c.radius * 0.6f, ky = c.cy + c.vector[1] * c.radius * 0.6f;
                fill.setColor(alpha(isHeld(c) ? 0x77ffffff : 0x44ffffff));
                canvas.drawCircle(kx, ky, c.radius * 0.42f, fill);
            } else if (c.kind == KIND_DPAD) {
                drawDpad(canvas, c);
            } else {
                drawCircle(canvas, c, (buttons & c.bit) != 0 ? 0x88ffffff : 0x30ffffff, true);
            }
        }
        if (editing && selected != null) {
            stroke.setColor(0xffffd54f);
            canvas.drawRect(selected.bounds, stroke);
        }
    }

    private void drawCircle(Canvas canvas, Control c, int color, boolean label) {
        fill.setColor(alpha(color));
        canvas.drawCircle(c.cx, c.cy, c.radius, fill);
        stroke.setColor(alpha(0x66ffffff));
        canvas.drawCircle(c.cx, c.cy, c.radius, stroke);
        if (label) {
            text.setColor(alpha(0xccffffff));
            canvas.drawText(c.label, c.cx, c.cy - (text.descent() + text.ascent()) / 2, text);
        }
    }

    private void drawDpad(Canvas canvas, Control c) {
        final float r = c.radius, a = r / 3;
        final int[] bits = {UP, DOWN, LEFT, RIGHT};
        final float[][] centers = {{c.cx, c.cy - 2 * a}, {c.cx, c.cy + 2 * a}, {c.cx - 2 * a, c.cy}, {c.cx + 2 * a, c.cy}};
        for (int i = 0; i < 4; i++) {
            fill.setColor(alpha((dpadBits & bits[i]) != 0 ? 0x88ffffff : 0x30ffffff));
            canvas.drawRect(centers[i][0] - a, centers[i][1] - a, centers[i][0] + a, centers[i][1] + a, fill);
            stroke.setColor(alpha(0x66ffffff));
            canvas.drawRect(centers[i][0] - a, centers[i][1] - a, centers[i][0] + a, centers[i][1] + a, stroke);
        }
    }

    private boolean isHeld(Control c) {
        for (Control h : held) if (h == c) return true;
        return false;
    }

    private Control controlAt(float x, float y) {
        if (toggle.hit(x, y)) return toggle;
        if (!visible) return null;
        // Сверху нарисованы последние элементы: ищем с конца.
        for (int i = controls.length - 1; i >= 0; i--) {
            final Control c = controls[i];
            if (c != toggle && c.on && c.hit(x, y)) return c;
        }
        return null;
    }

    // ---- Касания ----

    @Override public boolean onTouchEvent(MotionEvent event) {
        if (editing) return onEditTouch(event);
        final int action = event.getActionMasked();
        final int index = event.getActionIndex();
        boolean consumed = false;
        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                final int id = event.getPointerId(index);
                if (id >= held.length) break;
                final Control c = controlAt(event.getX(index), event.getY(index));
                if (c == null) {
                    if (touchMouse) consumed = mouseDown(event, index);
                    break;
                }
                consumed = true;
                if (c == toggle) {
                    visible = !visible;
                    preferences.edit().putBoolean("visible", visible).apply();
                    if (!visible) releaseAll();
                    break;
                }
                held[id] = c;
                track(c, event.getX(index), event.getY(index));
                break;
            }
            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < event.getPointerCount(); i++) {
                    final int id = event.getPointerId(i);
                    if (id < held.length && held[id] != null) {
                        track(held[id], event.getX(i), event.getY(i));
                        consumed = true;
                    }
                }
                if (mousePointer >= 0) consumed |= mouseMove(event);
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP: {
                final int id = event.getPointerId(index);
                if (id < held.length && held[id] != null) {
                    release(held[id]);
                    held[id] = null;
                    consumed = true;
                }
                if (id == mousePointer || id == mouseExtraPointer) consumed |= mouseUp(event, id);
                break;
            }
            case MotionEvent.ACTION_CANCEL:
                releaseAll();
                removeCallbacks(longPressRunnable);
                if (mousePointer >= 0) {
                    final int gx = toGameX(mouseStartX), gy = toGameY(mouseStartY);
                    direct(gx, gy, 1, false);
                }
                mousePointer = mouseExtraPointer = -1;
                mouseDragging = false;
                mouseMoved = mouseSecondFinger = false;
                consumed = true;
                break;
            default:
                break;
        }
        if (consumed) {
            recomputeButtons();
            send();
            invalidate();
        }
        return consumed;
    }

    // ---- Прямой тач (16:9 координаты, клики, перетаскивание, ПКМ) ----

    private int toGameX(float screenX) {
        final float w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0) return 960;
        final float canvasAspect = 1920f / 1080f;
        final float viewAspect = w / h;
        float dstW, dstX;
        if (viewAspect > canvasAspect) {
            dstW = h * canvasAspect;
            dstX = (w - dstW) * 0.5f;
        } else {
            dstW = w;
            dstX = 0f;
        }
        final float gx = (screenX - dstX) * 1920f / dstW;
        return Math.max(0, Math.min(1920, Math.round(gx)));
    }

    private int toGameY(float screenY) {
        final float w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0) return 540;
        final float canvasAspect = 1920f / 1080f;
        final float viewAspect = w / h;
        float dstH, dstY;
        if (viewAspect > canvasAspect) {
            dstH = h;
            dstY = 0f;
        } else {
            dstH = w / canvasAspect;
            dstY = (h - dstH) * 0.5f;
        }
        final float gy = (screenY - dstY) * 1080f / dstH;
        return Math.max(0, Math.min(1080, Math.round(gy)));
    }

    private final Runnable longPressRunnable = new Runnable() {
        @Override public void run() {
            if (mousePointer >= 0 && !mouseMoved && !mouseSecondFinger) {
                mouseSecondFinger = true;
                try {
                    performHapticFeedback(android.view.HapticFeedbackConstants.LONG_PRESS);
                } catch (Throwable ignored) {}
                final int gx = toGameX(mouseStartX), gy = toGameY(mouseStartY);
                direct(gx, gy, 1, false);
                direct(gx, gy, 2, true);
                direct(gx, gy, 2, false);
            }
        }
    };

    private boolean mouseDown(MotionEvent event, int index) {
        final int id = event.getPointerId(index);
        if (mousePointer >= 0) {
            if (mouseExtraPointer >= 0) return false;
            mouseExtraPointer = id;
            mouseSecondFinger = true;
            removeCallbacks(longPressRunnable);
            final int gx = toGameX(mouseStartX), gy = toGameY(mouseStartY);
            direct(gx, gy, 1, false);
            direct(gx, gy, 2, true);
            direct(gx, gy, 2, false);
            try {
                performHapticFeedback(android.view.HapticFeedbackConstants.CONTEXT_CLICK);
            } catch (Throwable ignored) {}
            return true;
        }
        mousePointer = id;
        mouseX = mouseStartX = event.getX(index);
        mouseY = mouseStartY = event.getY(index);
        mouseDownTime = event.getEventTime();
        mouseMoved = mouseSecondFinger = mouseDragging = false;
        final int gx = toGameX(mouseStartX), gy = toGameY(mouseStartY);
        direct(gx, gy, 1, true);
        postDelayed(longPressRunnable, longPressMs);
        return true;
    }

    private boolean mouseMove(MotionEvent event) {
        final int index = event.findPointerIndex(mousePointer);
        if (index < 0) return false;
        final float x = event.getX(index), y = event.getY(index);
        final float slop = 10f * getResources().getDisplayMetrics().density;
        if (!mouseMoved && Math.hypot(x - mouseStartX, y - mouseStartY) > slop) {
            mouseMoved = true;
            removeCallbacks(longPressRunnable);
        }
        if (!mouseSecondFinger) {
            final int gx = toGameX(x), gy = toGameY(y);
            direct(gx, gy, 1, true);
        }
        return true;
    }

    private boolean mouseUp(MotionEvent event, int id) {
        if (id == mouseExtraPointer) { mouseExtraPointer = -1; return true; }
        removeCallbacks(longPressRunnable);
        if (!mouseSecondFinger) {
            final int index = event.findPointerIndex(mousePointer);
            final float x = index >= 0 ? event.getX(index) : mouseStartX;
            final float y = index >= 0 ? event.getY(index) : mouseStartY;
            final int gx = toGameX(x), gy = toGameY(y);
            direct(gx, gy, 1, false);
        }
        mousePointer = -1;
        mouseMoved = mouseSecondFinger = mouseDragging = false;
        return true;
    }

    private static void direct(int gx, int gy, int button, boolean pressed) {
        try {
            android.util.Log.i("TouchControlsView", "direct: (" + gx + ", " + gy + ") btn=" + button + " pressed=" + pressed);
            GameActivity.nativeTouchDirect(gx, gy, button, pressed);
        } catch (Throwable t) {
            android.util.Log.e("TouchControlsView", "direct error: " + t.getMessage());
        }
    }

    // Редактор: касание выбирает элемент, перетаскивание двигает его; касание пустого места снимает выбор.
    private boolean onEditTouch(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: {
                final Control c = controlAt(event.getX(), event.getY());
                select(c);
                if (c != null) { dragDx = c.cx - event.getX(); dragDy = c.cy - event.getY(); }
                invalidate();
                return true;
            }
            case MotionEvent.ACTION_MOVE:
                if (selected != null && getWidth() > 0 && getHeight() > 0) {
                    selected.x = Math.max(0f, Math.min(1f, (event.getX() + dragDx) / getWidth()));
                    selected.y = Math.max(0f, Math.min(1f, (event.getY() + dragDy) / getHeight()));
                    layoutControls();
                    invalidate();
                }
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                if (selected != null && getWidth() > 0 && getHeight() > 0) {
                    // Хранится центр с учётом прижатия к краю.
                    selected.x = selected.cx / getWidth();
                    selected.y = selected.cy / getHeight();
                    save(selected);
                }
                return true;
            default:
                return true;
        }
    }

    private void track(Control c, float x, float y) {
        if (c.kind == KIND_STICK) {
            float dx = (x - c.cx) / (c.radius * 0.8f), dy = (y - c.cy) / (c.radius * 0.8f);
            final float len = (float) Math.sqrt(dx * dx + dy * dy);
            if (len > 1f) { dx /= len; dy /= len; }
            c.vector[0] = dx;
            c.vector[1] = dy;
        } else if (c.kind == KIND_DPAD) {
            final float dx = x - c.cx, dy = y - c.cy, dead = c.radius * 0.25f;
            int bits = 0;
            if (dy < -dead && Math.abs(dy) >= Math.abs(dx) * 0.5f) bits |= UP;
            if (dy > dead && Math.abs(dy) >= Math.abs(dx) * 0.5f) bits |= DOWN;
            if (dx < -dead && Math.abs(dx) >= Math.abs(dy) * 0.5f) bits |= LEFT;
            if (dx > dead && Math.abs(dx) >= Math.abs(dy) * 0.5f) bits |= RIGHT;
            dpadBits = bits;
        }
    }

    private void release(Control c) {
        if (c.kind == KIND_STICK) { c.vector[0] = 0; c.vector[1] = 0; }
        else if (c.kind == KIND_DPAD) dpadBits = 0;
    }

    private void releaseAll() {
        for (int i = 0; i < held.length; i++) held[i] = null;
        for (Control c : controls) { c.vector[0] = 0; c.vector[1] = 0; }
        dpadBits = 0;
        recomputeButtons();
        send();
        invalidate();
    }

    private void recomputeButtons() {
        int bits = dpadBits;
        for (Control h : held) if (h != null && h.kind == KIND_BUTTON) bits |= h.bit;
        buttons = bits;
    }

    private static int axis(float value) {
        return Math.max(0, Math.min(255, Math.round(128 + value * 127)));
    }

    private void send() {
        final int lx = axis(leftStick.vector[0]), ly = axis(leftStick.vector[1]);
        final int rx = axis(rightStick.vector[0]), ry = axis(rightStick.vector[1]);
        final int sticks = lx | ly << 8 | rx << 16 | ry << 24;
        if (buttons == sentButtons && sticks == sentSticks) return;
        sentButtons = buttons; sentSticks = sticks;
        try {
            GameActivity.nativeVirtualPad(buttons, lx, ly, rx, ry);
        } catch (UnsatisfiedLinkError ignored) {
        }
    }
}
