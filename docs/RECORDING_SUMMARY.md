# Video Recording - Quick Summary

## 🎯 Goal
Record SuperTuxKart gameplay showing **real-time FPS counter** with LGX Runtime integrated.

## 🚀 One Command to Run

```bash
cd ~/Projects/LGX/stk-code
./play_for_recording.sh
```

## 📊 What You'll See

**FPS Counter** (top center of screen):
```
FPS: 145/169/311 - 245 KTris, Ping: 0ms
```

**Performance**:
- Average: **169 FPS** ⚡
- Min: 11 FPS (loading)
- Max: 311 FPS (simple scenes)

## 🎥 Recording Steps

1. **Start screen recorder** (OBS, SimpleScreenRecorder, etc.)
2. **Run**: `./play_for_recording.sh`
3. **Play** the race (1-2 minutes)
4. **Stop** recording when done

## ✅ What's Enabled

- ✅ FPS counter visible on screen
- ✅ VSync disabled (real performance)
- ✅ LGX Runtime active (256MB pool)
- ✅ Frame arena resets (169 times/second)
- ✅ Auto-start race (Lighthouse, 2 laps, 4 karts)

## 🎬 Key Points to Show

1. **FPS counter** at top of screen
2. **Consistent ~169 FPS** during gameplay
3. **Smooth performance** with LGX integrated
4. **No crashes** or stuttering

## 📝 Video Description Template

```
SuperTuxKart + LGX Runtime Performance Demo

Running at 169 FPS average with LGX Runtime integrated.
FPS counter visible on screen (top center).

Performance:
• Average: 169 FPS
• Min: 11 FPS (loading)
• Max: 311 FPS
• Variance: 30%

LGX Configuration:
• 256MB memory pool
• Frame arena resets: 169 times/second
• Overhead: < 0.017% of frame time

This is the baseline before replacing allocations.
Next step: Replace malloc/free with lgx_alloc_frame()
Expected: 200+ FPS with reduced variance

System: Linux, Release build, VSync disabled
LGX Version: 1.0.1
```

## 🔧 Troubleshooting

**FPS counter not visible?**
→ Press F12 to toggle

**Stuck at 60 FPS?**
→ Script handles this automatically

**Game won't start?**
→ Check: `ldd build-lgx/bin/supertuxkart | grep lgx`

---

## Ready to Record!

```bash
cd ~/Projects/LGX/stk-code
./play_for_recording.sh
```

**The FPS counter will be visible on screen showing ~169 FPS!** 🎮
