package main

import (
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io/fs"
	"net"
	"os"
	"os/exec"
	"os/signal"
	"runtime"
	"slices"
	"strings"
	"syscall"
	"time"
)

const isLinux bool = runtime.GOOS == "linux"
const isWindows bool = runtime.GOOS == "windows"

const DEFAULT_SOCKET_PATH string = "/tmp/snackdaemon.sock"

var DEFAULT_LINUX_SHELL []string = []string{"bash", "-c"}
var DEFAULT_WINDOWS_SHELL []string = []string{"powershell.exe", "-c"}

func ternary[T any](isA bool, a T, b T) T {
	if isA {
		return a
	}
	return b
}

type Timer struct {
	sleepTime time.Duration
	callback  func()
	started   bool
	stopped   bool
}

func (timer *Timer) start() {
	timer.started = true
	time.NewTimer(timer.sleepTime)
	go func() {
		time.Sleep(timer.sleepTime)
		if !timer.stopped {
			timer.callback()
		}
		timer.stopped = true
	}()
}

func (timer *Timer) cancel() {
	timer.stopped = true
}

func NewTimer(sleepTime time.Duration, callback func(), autoStart bool) *Timer {
	timer := Timer{sleepTime: sleepTime, callback: callback, started: false, stopped: false}
	if autoStart {
		timer.start()
	}
	return &timer
}

type TlvData struct {
	Type  uint16
	Value string
}

func (tlv TlvData) toBytes() []byte {
	valueInByte := []byte(tlv.Value)
	var length uint64 = uint64(len(valueInByte))

	buffer := make([]byte, 10, 11+length)

	binary.BigEndian.PutUint16(buffer[0:], tlv.Type)
	binary.BigEndian.PutUint64(buffer[2:], length)
	buffer = append(buffer, valueInByte...)
	buffer = append(buffer, 0)

	return buffer
}

func decodeToTlvData(bytes []byte) (TlvData, error) {
	var tlv TlvData

	if len(bytes) < 11 || bytes[len(bytes)-1] != byte(0) {
		return tlv, errors.New("this is a invalid tlv data")
	}

	length := binary.BigEndian.Uint64(bytes[2:])
	if length+11 != uint64(len(bytes)) {
		return tlv, errors.New("this is a invalid tlv data")
	}

	value := string(bytes[10 : len(bytes)-1])

	if strings.Contains(value, string(rune(0))) {
		return tlv, errors.New("this is a invalid tlv data")
	}

	tlv.Type = binary.BigEndian.Uint16(bytes[0:])
	tlv.Value = value

	return tlv, nil
}

func printHelp() {
	// TODO: update with new flags
	fmt.Println("usage: snackdaemon <command> [<args>]")
	fmt.Println("commands:")

	fmt.Printf("    %-16sPrint help\n", "help")
	fmt.Printf("    %-16sStart the daemon\n", "daemon")
	fmt.Printf("    %-16sreload the config\n", "reload")
	fmt.Printf("    %-16sSend \"kill\" to the daemon\n", "kill")
	fmt.Printf("    %-16sPing the daemon to check connectivity\n", "ping")
	fmt.Printf("    %-16sUpdate with <arg>'s index in \"options\" in config file\n", "update <arg>")
	fmt.Printf("    %-16sTrigger the \"closeCommand\" in config file and end timer\n", "close")

	fmt.Println()
	fmt.Println("Visit 'https://github.com/Shiphan/snackdaemon' for more information or bug report.")

}

func printInvalidArgs() {
	fmt.Println("invalid arguments, try `snackdaemon help` to get help.")
}

func printInvalidConfig() {
	fmt.Println("invalid config file")
}

const (
	ERROR   = 0
	RESPOND = 1
	PING    = 2
	UPDATE  = 3
	CLOSE   = 4
	RELOAD  = 5
	KILL    = 6
)

func recvTlv(conn net.Conn) (TlvData, error) {
	var recv TlvData

	tlBuffer := make([]byte, 10)
	_, err := conn.Read(tlBuffer)
	if err != nil {
		return recv, err
	}

	length := binary.BigEndian.Uint64(tlBuffer[2:])

	vBuffer := make([]byte, length+1)
	_, err = conn.Read(vBuffer)
	if err != nil {
		return recv, err
	}

	recv, err = decodeToTlvData(append(tlBuffer, vBuffer...))
	if err != nil {
		return recv, err
	}

	return recv, nil

}

func client(sendTlv TlvData, socketPath string) (TlvData, error) {
	var recv TlvData
	conn, err := net.Dial("unix", socketPath)
	if err != nil {
		return recv, err
	}
	defer conn.Close()

	conn.Write([]byte(sendTlv.toBytes()))

	recv, err = recvTlv(conn)
	if err != nil {
		return recv, err
	}

	return recv, nil
}

type Config struct {
	timeoutDuration time.Duration
	Timeout         string   `json:"timeout"`
	OpenCommand     string   `json:"openCommand"`
	UpdateCommand   string   `json:"updateCommand"`
	CloseCommand    string   `json:"closeCommand"`
	Options         []string `json:"options"`
}

func loadConfig(configPath string) (Config, error) {
	var config Config

	configFile, err := os.ReadFile(configPath)
	if err != nil {
		return Config{}, err
	}

	err = json.Unmarshal(configFile, &config)
	if err != nil {
		return Config{}, err
	}
	config.timeoutDuration, err = time.ParseDuration(config.Timeout)
	if err != nil {
		return Config{}, err
	}

	return config, nil
}

func printConfig(config Config) {
	fmt.Println("config:")
	fmt.Printf("timeout: %v\n", config.timeoutDuration.String())
	fmt.Printf("open command: %v\n", config.OpenCommand)
	fmt.Printf("update command: %v\n", config.UpdateCommand)
	fmt.Printf("close command: %v\n", config.CloseCommand)
	fmt.Printf("options: %v\n", config.Options)
}

func execute(commands []string) {
	exec.Command(commands[0], commands[1:]...).Run()
}

func openDaemon(socketPath string, useConfig bool, configPath string) {
	if !useConfig {
		homedir, err := os.UserHomeDir()
		if err != nil {
			fmt.Println("Can not get your home directory.")
			return
		}
		configPath = homedir + "/.config/snackdaemon/snackdaemon.json"
	}

	if _, err := os.Stat(socketPath); err == nil {
		fmt.Printf("Found \"%v\", trying close it.\n", socketPath)

		for i := 0; i < 5; i++ {
			recv, err := client(TlvData{KILL, ""}, socketPath)
			if err != nil {
				fmt.Printf("try[%d]: error: %v\n", i, err)
				time.Sleep(500 * time.Millisecond)
				if _, err := os.Stat(socketPath); errors.Is(err, fs.ErrNotExist) {
					break
				}
				continue
			}

			fmt.Printf("try[%d]: received: %v\n", i, recv.Value)

			if _, err := os.Stat(socketPath); errors.Is(err, fs.ErrNotExist) {
				break
			}
			time.Sleep(500 * time.Millisecond)
			if _, err := os.Stat(socketPath); errors.Is(err, fs.ErrNotExist) {
				break
			}
		}

		if _, err := os.Stat(socketPath); err == nil {
			fmt.Printf("Unable to close the old daemon (socket at \"%v\")\n", socketPath)
			return
		}

		fmt.Println("----------")
	}

	config, err := loadConfig(configPath)
	if err != nil {
		printInvalidConfig()
		return
	}

	printConfig(config)
	fmt.Println("----------")

	listener, err := net.Listen("unix", socketPath)
	if err != nil {
		fmt.Printf("Can not listen to \"%s\"", socketPath)
	}
	defer os.Remove(socketPath)
	defer listener.Close()

	c := make(chan os.Signal, 1)
	signal.Notify(c, syscall.SIGINT)
	go func() {
		<-c
		_, err := client(TlvData{KILL, ""}, socketPath)
		if err != nil {
			panic("Unable to connect to daemon when received SIGINT.")
		}
		time.Sleep(time.Second)
		fmt.Println("Force closing...")
		os.Remove(socketPath)
		os.Exit(0)
	}()

	timer := NewTimer(config.timeoutDuration, func() {}, false)
	running := true
	for running {
		conn, err := listener.Accept()
		if err != nil {
			fmt.Println("Can not accept this")
			return
		}
		defer conn.Close()

		tlv, err := recvTlv(conn)
		if err != nil {
			continue
		}

		switch tlv.Type {
		case PING:
			fmt.Println("ping")

			conn.Write(TlvData{Type: RESPOND, Value: "pong"}.toBytes())
		case KILL:
			fmt.Println("kill")

			running = false

			conn.Write(TlvData{Type: RESPOND, Value: "ok"}.toBytes())
		case CLOSE:
			fmt.Println("close")

			if timer.started && !timer.stopped {
				timer.cancel()
				execute(append(ternary(isWindows, DEFAULT_WINDOWS_SHELL, DEFAULT_LINUX_SHELL), config.CloseCommand))
			}

			conn.Write(TlvData{Type: RESPOND, Value: ""}.toBytes())
		case UPDATE:
			index := slices.Index(config.Options, tlv.Value)
			if index == -1 {
				fmt.Printf("update: %s (no such option)\n", tlv.Value)
				conn.Write(TlvData{Type: RESPOND, Value: "no such option"}.toBytes())
				continue
			}

			if timer.stopped || !timer.started {
				execute(append(ternary(isWindows, DEFAULT_WINDOWS_SHELL, DEFAULT_LINUX_SHELL), config.OpenCommand))
				exec.Command("bash", "-c", config.OpenCommand).Run()
			}

			execute(append(ternary(isWindows, DEFAULT_WINDOWS_SHELL, DEFAULT_LINUX_SHELL), fmt.Sprintf(config.UpdateCommand, index)))

			fmt.Printf("update: %s (index: %d)\n", tlv.Value, index)

			timer.cancel()
			timer = NewTimer(config.timeoutDuration, func() {
				execute(append(ternary(isWindows, DEFAULT_WINDOWS_SHELL, DEFAULT_LINUX_SHELL), config.CloseCommand))
			}, true)

			conn.Write(TlvData{Type: RESPOND, Value: ""}.toBytes())
		case RELOAD:
			newConfigPath := tlv.Value
			if newConfigPath == "" {
				newConfigPath = configPath
			}
			newConfig, err := loadConfig(newConfigPath)
			if err != nil {
				fmt.Printf("reload: failed to reload with \"%v\"\n", newConfigPath)
				conn.Write(TlvData{Type: RESPOND, Value: "failed to reload"}.toBytes())
				continue
			}

			config = newConfig
			configPath = newConfigPath

			fmt.Printf("reload: reload with \"%v\"\n", newConfigPath)
			fmt.Println("----------")
			printConfig(config)
			fmt.Println("----------")

			conn.Write(TlvData{Type: RESPOND, Value: "ok"}.toBytes())
		default:
			fmt.Printf("Unknown message: %v\n", tlv)
		}
	}
}

type Flags struct {
	help       bool
	socket     bool
	socketPath string
	config     bool
	configPath string
}

func main() {
	/*
		tags: --help -h --socket -s --config -c
		commands: daemon kill ping close reload update help
	*/

	flags := Flags{help: false, socket: false, config: false}
	command := ""
	updateOption := ""
	for i := 1; i < len(os.Args); i++ {
		switch os.Args[i] {
		case "--help", "-h":
			if flags.help {
				printInvalidArgs()
				return
			}

			flags.help = true
		case "--socket", "-s":
			i++
			if flags.socket || i >= len(os.Args) {
				printInvalidArgs()
				return
			}

			flags.socket = true
			flags.socketPath = os.Args[i]
		case "--config", "-c":
			i++
			if flags.config || i >= len(os.Args) {
				printInvalidArgs()
				return
			}

			flags.config = true
			flags.configPath = os.Args[i]
		case "daemon", "kill", "ping", "reload", "close", "generate-config", "help":
			if command != "" {
				printInvalidArgs()
				return
			}

			command = os.Args[i]
		case "update":
			i++
			if command != "" || i >= len(os.Args) {
				printInvalidArgs()
				return
			}

			command = "update"
			updateOption = os.Args[i]
		default:
			printInvalidArgs()
			return
		}
	}

	switch command {
	case "":
		if flags.socket || flags.config {
			printInvalidArgs()
			return
		}
		if flags.help {
			printHelp()
			return
		}

		printInvalidArgs()
	case "daemon":
		if flags.help {
			// TODO: add daemon help
			fmt.Println("The help for daemon")
			return
		}

		openDaemon(ternary(flags.socket, flags.socketPath, DEFAULT_SOCKET_PATH), flags.config, flags.configPath)
	case "kill":
		if flags.config {
			printInvalidArgs()
			return
		}
		if flags.help {
			// TODO: add kill help
			fmt.Println("The help for kill")
			return
		}

		recv, err := client(TlvData{KILL, ""}, ternary(flags.socket, flags.socketPath, DEFAULT_SOCKET_PATH))
		if err != nil {
			fmt.Println("Unable to connect to daemon.")
			break
		}
		fmt.Println(recv.Value)
	case "ping":
		if flags.config {
			printInvalidArgs()
			return
		}
		if flags.help {
			// TODO: add ping help
			fmt.Println("The help for ping")
			return
		}

		start := time.Now()
		recv, err := client(TlvData{PING, ""}, ternary(flags.socket, flags.socketPath, DEFAULT_SOCKET_PATH))
		if err != nil {
			fmt.Println("Unable to connect to daemon.")
			break
		}
		end := time.Now()
		fmt.Printf("%v (latency: %s)\n", recv.Value, end.Sub(start).String())
	case "reload":
		if flags.help {
			// TODO: add reload help
			fmt.Println("The help for reload")
			return
		}

		recv, err := client(TlvData{RELOAD, flags.configPath}, ternary(flags.socket, flags.socketPath, DEFAULT_SOCKET_PATH))
		if err != nil {
			fmt.Println("Unable to connect to daemon.")
			break
		}
		fmt.Println(recv.Value)
	case "close":
		if flags.config {
			printInvalidArgs()
			return
		}
		if flags.help {
			// TODO: add close help
			fmt.Println("The help for close")
			return
		}

		recv, err := client(TlvData{CLOSE, ""}, ternary(flags.socket, flags.socketPath, DEFAULT_SOCKET_PATH))
		if err != nil {
			fmt.Println("Unable to connect to daemon.")
			break
		}
		fmt.Println(recv.Value)
	case "update":
		if flags.config {
			printInvalidArgs()
			return
		}
		if flags.help {
			// TODO: add update help
			fmt.Println("The help for update")
			return
		}

		recv, err := client(TlvData{UPDATE, updateOption}, ternary(flags.socket, flags.socketPath, DEFAULT_SOCKET_PATH))
		if err != nil {
			fmt.Println("Unable to connect to daemon.")
			break
		}
		fmt.Println(recv.Value)
	case "generate-config":
		if flags.config || flags.socket {
			printInvalidArgs()
			return
		}
		if flags.help {
			// TODO: add generate-config help
			fmt.Println("The help for generate-config")
			return
		}

		b, err := json.MarshalIndent(Config{Timeout: "2s", OpenCommand: "eww open snackbar", UpdateCommand: "eww update snackbarIndex=%d", CloseCommand: "eww close snackbar", Options: []string{"volume", "player", "screenbrightness", "powerprofiles"}}, "", "\t")
		if err != nil {
			fmt.Println(err)
		}
		fmt.Println(string(b))
	case "help":
		if flags.help {
			// TODO: add help help
			fmt.Println("usage: snackdaemon help")
			fmt.Println("Print help")
			return
		}

		printHelp()
	default:
		printInvalidArgs()
		return
	}
}
