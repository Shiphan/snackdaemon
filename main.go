package main

import (
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"net"
	"os"
	"os/exec"
	"runtime"
	"slices"
	"strconv"
	"strings"
	"time"
)

const isLinux bool = runtime.GOOS == "linux"
const isWindows bool = runtime.GOOS == "windows"

const DEFAULT_PORT uint16 = 42069

var DEFAULT_LINUX_SHELL []string = []string{"bash", "-c"}
var DEFAULT_WINDOWS_SHELL []string = []string{"powershell.exe", "-c"}

func cond[T any](isA bool, a T, b T) T {
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

func client(sendTlv TlvData, port uint16) (TlvData, error) {
	var recv TlvData
	conn, err := net.Dial("tcp", fmt.Sprintf(":%v", port))
	if err != nil {
		return recv, err
	}
	defer conn.Close()

	conn.Write(sendTlv.toBytes())

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

func openDaemon(port uint16, configPath string) error {
	config, err := loadConfig(configPath)
	if err != nil {
		return fmt.Errorf("error: invalid config file (%v)", err)
	}

	printConfig(config)
	fmt.Println("----------")

	listener, err := net.Listen("tcp", fmt.Sprintf(":%v", port))
	if err != nil {
		return fmt.Errorf("error: can not listen to port %v (%v)", port, err)
	}
	defer listener.Close()

	timer := NewTimer(config.timeoutDuration, func() {}, false)
	running := true
	for running {
		func() {
			conn, err := listener.Accept()
			if err != nil {
				log.Printf("Can not accept this (%v)", err)
				return
			}
			defer conn.Close()

			tlv, err := recvTlv(conn)
			if err != nil {
				return
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
					execute(append(cond(isWindows, DEFAULT_WINDOWS_SHELL, DEFAULT_LINUX_SHELL), config.CloseCommand))
				}

				conn.Write(TlvData{Type: RESPOND, Value: ""}.toBytes())
			case UPDATE:
				index := slices.Index(config.Options, tlv.Value)
				if index == -1 {
					fmt.Printf("update: %s (no such option)\n", tlv.Value)
					conn.Write(TlvData{Type: RESPOND, Value: "no such option"}.toBytes())
					return
				}

				if timer.stopped || !timer.started {
					execute(append(cond(isWindows, DEFAULT_WINDOWS_SHELL, DEFAULT_LINUX_SHELL), config.OpenCommand))
				}

				execute(append(cond(isWindows, DEFAULT_WINDOWS_SHELL, DEFAULT_LINUX_SHELL), fmt.Sprintf(config.UpdateCommand, index)))

				fmt.Printf("update: %s (index: %d)\n", tlv.Value, index)

				timer.cancel()
				timer = NewTimer(config.timeoutDuration, func() {
					execute(append(cond(isWindows, DEFAULT_WINDOWS_SHELL, DEFAULT_LINUX_SHELL), config.CloseCommand))
				}, true)

				conn.Write(TlvData{Type: RESPOND, Value: ""}.toBytes())
			case RELOAD:
				newConfigPath := tlv.Value
				if newConfigPath == "" {
					newConfigPath = configPath
				}
				newConfig, err := loadConfig(newConfigPath)
				if err != nil {
					log.Printf("reload: failed to reload with \"%v\"\n", newConfigPath)
					conn.Write(TlvData{Type: RESPOND, Value: "failed to reload"}.toBytes())
					return
				}

				config = newConfig
				configPath = newConfigPath

				fmt.Printf("reload: reload with \"%v\"\n", newConfigPath)
				fmt.Println("----------")
				printConfig(config)
				fmt.Println("----------")

				conn.Write(TlvData{Type: RESPOND, Value: "ok"}.toBytes())
			default:
				log.Printf("Unknown message: %v\n", tlv)
			}
		}()
	}
	return nil
}

type Args struct {
	command      string
	help         bool
	port         uint16
	configPath   string
	updateOption string
}

// tags: --help -h --port -p --config -c
// commands: daemon kill ping close reload update help
func getArgs() (Args, error) {
	tmpArgs := struct {
		command      string
		help         bool
		port         *uint16
		configPath   *string
		updateOption string
	}{
		command:      "",
		help:         false,
		port:         nil,
		configPath:   nil,
		updateOption: "",
	}
	for i := 1; i < len(os.Args); i++ {
		switch os.Args[i] {
		case "--help", "-h":
			if tmpArgs.help {
				return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
			}

			tmpArgs.help = true
		case "--port", "-p":
			i++
			if tmpArgs.port != nil || i >= len(os.Args) {
				return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
			}

			port, err := strconv.ParseUint(os.Args[i], 10, 16)
			if err != nil {
				return Args{}, err
			}
			tmpArgs.port = new(uint16)
			*tmpArgs.port = uint16(port)
		case "--config", "-c":
			i++
			if tmpArgs.configPath != nil || i >= len(os.Args) {
				return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
			}

			tmpArgs.configPath = &os.Args[i]
		case "daemon", "kill", "ping", "reload", "close", "generate-config", "help":
			if tmpArgs.command != "" {
				return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
			}

			tmpArgs.command = os.Args[i]
		case "update":
			i++
			if tmpArgs.command != "" || i >= len(os.Args) {
				return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
			}

			tmpArgs.command = "update"
			tmpArgs.updateOption = os.Args[i]
		default:
			return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
		}
	}

	if tmpArgs.command == "" {
		return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
	}
	args := Args{
		command:      tmpArgs.command,
		help:         tmpArgs.help,
		port:         DEFAULT_PORT,
		updateOption: tmpArgs.updateOption,
	}
	if tmpArgs.port != nil {
		args.port = *tmpArgs.port
	}
	if tmpArgs.configPath != nil {
		args.configPath = *tmpArgs.configPath
	} else {
		homedir, err := os.UserHomeDir()
		if err != nil {
			return Args{}, err
		}
		args.configPath = homedir + "/.config/snackdaemon/snackdaemon.json"
	}

	return args, nil
}

func main() {
	args, err := getArgs()
	if err != nil {
		fmt.Println(err)
		return
	}

	switch args.command {
	case "daemon":
		if args.help {
			// TODO: add daemon help
			fmt.Println("The help for daemon")
			return
		}

		err := openDaemon(args.port, args.configPath)
		if err != nil {
			log.Fatal(err)
		}
	case "kill":
		if args.help {
			// TODO: add kill help
			fmt.Println("The help for kill")
			return
		}

		recv, err := client(TlvData{KILL, ""}, args.port)
		if err != nil {
			log.Fatalf("Unable to connect to daemon. (%v)", err)
		}
		fmt.Println(recv.Value)
	case "ping":
		if args.help {
			// TODO: add ping help
			fmt.Println("The help for ping")
			return
		}

		start := time.Now()
		recv, err := client(TlvData{PING, ""}, args.port)
		if err != nil {
			log.Fatalf("Unable to connect to daemon. (%v)", err)
		}
		end := time.Now()
		fmt.Printf("%v (latency: %s)\n", recv.Value, end.Sub(start).String())
	case "reload":
		if args.help {
			// TODO: add reload help
			fmt.Println("The help for reload")
			return
		}

		recv, err := client(TlvData{RELOAD, args.configPath}, args.port)
		if err != nil {
			log.Fatalf("Unable to connect to daemon. (%v)", err)
		}
		fmt.Println(recv.Value)
	case "close":
		if args.help {
			// TODO: add close help
			fmt.Println("The help for close")
			return
		}

		recv, err := client(TlvData{CLOSE, ""}, args.port)
		if err != nil {
			log.Fatalf("Unable to connect to daemon. (%v)", err)
		}
		fmt.Println(recv.Value)
	case "update":
		if args.help {
			// TODO: add update help
			fmt.Println("The help for update")
			return
		}

		recv, err := client(TlvData{UPDATE, args.updateOption}, args.port)
		if err != nil {
			log.Fatalf("Unable to connect to daemon. (%v)", err)
		}
		fmt.Println(recv.Value)
	case "generate-config":
		if args.help {
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
		if args.help {
			// TODO: add help help
			fmt.Println("usage: snackdaemon help")
			fmt.Println("Print help")
			return
		}

		printHelp()
	default:
		fmt.Println("invalid arguments, try `snackdaemon help` to get help.")
	}
}
