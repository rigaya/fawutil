# fawutil

QSVEnc/NVEnc/VCEEncにFakeAACWave(FAW)→aacの処理を内蔵するにあたり、既存のFAW.exe/fawcl.exeと同等であろうと思われる処理の再現を試み、検証したものです。

基本的には同等の出力が得られるはずですが、FAW.exe/fawcl.exeを分析できていない箇所もあり、完全な一致は保証しません。

特にaac→fawの処理においては、FAW.exe/fawcl.exeと完全には一致しません。また入力のaacに欠損がある場合、結果が異なる可能性が高いです。

ただ、確認した限り実用上問題ありません。

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

input.wavがFAW half size mixの場合は、2つのaacが出力されます。

このモードでは、delay等の補正は行いません。


### aac -> faw(wav)
```
fawutil [-E] [-sn] [-dxxx] input.aac [input2.aac] [output.wav]
  n = 1 or 2 (1:1/1 2:1/2)
  xxx = ms単位
```

2重音声など場合に、2つのaacを入力とすると(```input2.aac```を指定した場合)、FAW half size mix処理を行います。このfawをaacに戻すことで、2重音声をそれぞれ無劣化で取り出すことができます。

入力ファイルはADTS AACである必要があります。特にチェックしていないので、異なる形式の場合には意図しない結果になると思います。

delayについては明示的に指定できるほか、aacファイル名の "DELAY xxxms" 部分を自動的に読み取り反映します。


## fawcl.exe との差異

- FAW half size mix処理に対応し、2重音声を扱うことができます。
- 入出力を標準入力/標準出力とすることが可能です。
- データの完全な一致は保証しません。

## fawutil 使用にあたっての注意事項  
無保証です。自己責任で使用してください。   
fawutilを使用したことによる、いかなる損害・トラブルについても責任を負いません。 

## fawutilのソースコードについて
- MITライセンスです。