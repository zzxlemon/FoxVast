# FoxVast Linux 鏋勫缓

鏈洰褰曟槸 FoxVast 鐨?Linux 绉绘鐗?涓?`windows/` 骞跺垪,婧愮爜涓?UTF-8 缂栫爜)銆?
## 渚濊禆(Ubuntu/Debian)

```bash
sudo apt install build-essential cmake libglfw3-dev libcurl4-openssl-dev
```

鍏朵粬鍙戣鐗堝搴斿寘鍚?
- Fedora: `dnf install gcc-c++ cmake glfw-devel libcurl-devel`
- Arch: `pacman -S base-devel cmake glfw curl`

## 鏋勫缓

```bash
mkdir -p build && cd build
cmake ../FoxCore -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

(椤跺眰鍏ュ彛鏄?`FoxCore/CMakeLists.txt`,瀹冧細寮曞叆 `dlls/` 涓?`Package/`銆?

浜х墿:
- `core/fox` 鈥?瑙ｉ噴鍣?缂栬瘧鍣?fox 鍙墽琛屾枃浠?
- `core/fox.*.so` 鈥?杩愯鏃跺簱(math/random/file/util/time/socket;graphics 闇€瑕?glfw 宸插畨瑁?

## 杩愯

```bash
cd core
./fox -f path/to/script.fox    # 瑙ｉ噴鍣?./fox -c path/to/script.fox    # 缂栬瘧 .fc
./fox -fc path/to/script.fox   # 瀛楄妭鐮?VM
./fox -version
```

## 搴撳畨瑁呬綅缃?
`!import flog` 鎸変互涓嬮『搴忔煡鎵?`flog.fox`:
1. 鑴氭湰鎵€鍦ㄧ洰褰?2. `~/.foxlibs/`(鍙敤 `FOXLIB_PATH` 鐜鍙橀噺瑕嗙洊涓哄叾浠栫洰褰?
3. `/usr/local/lib/foxlibs/`
4. 褰撳墠宸ヤ綔鐩綍

(Windows 鐗堟煡鎵?`C:\FoxLibs\`,涓よ€呬笉閫氱敤銆?

## Linux 骞冲彴璇存槑

- 婧愮爜涓?UTF-8(Windows 鐗堜负 GBK),娉ㄩ噴涓庡瓧绗︿覆鐩存帴浣跨敤 UTF-8
- 鍏ㄥ眬鐑敭(`register_hotkeys`)涓?OS 绾х湡瀹炵偣鍑?`simulate_real_click`)鍦?Linux 涓婁笉鏀寔,璋冪敤浼氭姏杩愯鏃堕敊璇?- 鍏朵粬鍥惧舰 API(GLFW 绐楀彛銆佺粯鍒躲€佽创鍥俱€佹枃鏈?鍧囧彲鐢?- 瀛楄妭鐮?`.fc` 璺ㄥ钩鍙板吋瀹?涓?Windows 鐗堜骇鐗╀竴鑷?
