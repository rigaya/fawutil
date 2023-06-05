# fawutil

QSVEnc/NVEnc/VCEEncにFakeAACWave(FAW)→aacの処理を内蔵するにあたり、既存のFAW.exe/fawcl.exeと同等であろうと思われる処理を実装し、検証したものです。

基本的には同等の出力が得られるはずですが、FAW.exe/fawcl.exeと完全に一致することは保証できません。特に入力のaacに欠損がある場合、対処法等が異なる可能性が高いです。

## FakeAACWave(FAW)とは

FakeAACWave(FAW)は、ADTS AACをwavファイルに埋め込んだものです。

Aviutl等でフレーム単位でwavとして編集を行い、編集後にaacに戻すことで、無劣化での音声の編集を実現します。

## 想定動作環境  
Windows 10/11 (x86/x64)  

## 使用方法

### faw(wav) -> aac
```
fawutil [-D] input.wav [output.aac]
```

### aac -> faw(wav)
```
fawutil [-E] [-sn] [-dxxx] input.aac [output.wav]
  n = 1 or 2 (1:1/1 2:1/2)
  xxx = ms単位
```
また、aacファイル名の "DELAY xxxms" 部分を自動的に読み取り反映することができます。

## fawutil 使用にあたっての注意事項  
無保証です。自己責任で使用してください。   
fawutilを使用したことによる、いかなる損害・トラブルについても責任を負いません。 

## fawutilのソースコードについて
- MITライセンスです。