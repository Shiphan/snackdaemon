# snackdaemon
這個程式是以管理用eww寫的snackbar為目的設計的

[This project is aim to control a snackbar in eww, but it should work with other things that can be controlled by simple commands.]: # 

## Example
有幾個command可以使用

[There're three commands you can use.]: #

```bash
snackdaemon update option
snackdaemon close
snackdaemon help
```

---

第一次使用`snackdaemon update option`時會執行openCommand，並試著在options尋找符合option的項目，如果有找到的話，則使用該項的index替換掉updateCommand中的`{}`，然後執行。  
之後的每次update都是像第一次一樣，只是不會執行openCommand。  
當最後一次update後經過timeout(以ms計)設定的時間、或是執行了`snackdaemon close`後，closeCommand會被執行，並重新開始新的循環。

[The first time you run `snackdaemon update something`, the `openCommand` will be executed. Then, it will try to find the match one of "something" in options. If found, use it's index to replace `{}` in `updateCommand`, and then execute it.  
Every following update is just like the first one, except that only the update part will be executed.
When the time set by `timeout` (in ms) has passed after the last update, or after you run `snackdaemon close`, `closeCommand` will be executed and next time it will start form the beginning.
]: #

---

可以參考以下的conf檔
```snackdaemon.conf
timeout = 2000
openCommand = eww open snackbar > /dev/null
updateCommand = eww update snackbarIndex={} > /dev/null
closeCommand = eww close snackbar > /dev/null
options = [
	volume
	player
	screenbrightness
	powerprofiles
]
```

## daemon...?
我知道，這其實不是一個"daemon"，根本就沒有一個行程在背景運行，而是用shared memory實現這些功能的，這主要是我的skill issue(我想這應該是需要Inter-process communication)。  
總之，在接下來的版本中，我們會先用daemon實現與現在相同的功能，有可能會使用的別的語言，之後再試著增加一些新的功能。

[It's just skill issue.]: #