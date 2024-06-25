# snackdaemon
這個程式是以管理用eww寫的snackbar為目的設計的

[This project is aim to control a snackbar in eww, but it should work with other things that can be controlled by simple commands.]: # 

## go branch
這個分支以go實現類似的功能，有一些額外的改進:
* json: 改用json作為config檔的格式
* reload: 在運行中重新載入cofig檔
* ping: 現在ping可以計算延遲的時間

## Example
* [My eww snackbar](https://github.com/Shiphan/Dotfiles)

[demo.webm](https://github.com/Shiphan/snackdaemon/assets/140245703/270afdd5-f62d-458a-9bc2-1fbb979074b5)

## Install
### Linux
```bash
git pull https://github.com/Shiphan/snackdaemon.git
cd snackdaemon
go install
```

## Features
有幾個command可以使用

[There're three commands you can use.]: #

```bash
snackdaemon daemon
snackdaemon reload
snackdaemon kill
snackdaemon ping
snackdaemon update <option>
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

可以參考以下的config(json)檔
```json
{
	"timeout": "2s",
	"openCommand": "eww open snackbar",
	"updateCommand": "eww update snackbarIndex=%d",
	"closeCommand": "eww close snackbar",
	"options": [
		"volume",
		"player",
		"screenbrightness",
		"powerprofiles"
	]
}
```