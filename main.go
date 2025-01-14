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
	"path/filepath"
	"slices"
	"strings"
	"time"
)

const DEFAULT_SOCKET_ADDRESS string = "\x00snackdaemon"

// var DEFAULT_SOCKET_ADDRESS string = fmt.Sprintf("/run/user/%d/snackdaemon/snackdaemon.sock", os.Getuid())

var DEFAULT_SHELL []string = []string{"bash", "-c"}

type Timer struct {
	sleepTime time.Duration
	callback  func()
	stopped   bool
}

func (timer *Timer) Cancel() {
	timer.stopped = true
}

func (timer *Timer) Stopped() bool {
	return timer.stopped
}

func NewTimer(sleepTime time.Duration, callback func()) *Timer {
	timer := Timer{sleepTime: sleepTime, callback: callback, stopped: false}
	go func() {
		time.Sleep(timer.sleepTime)
		if !timer.stopped {
			timer.callback()
		}
		timer.stopped = true
	}()
	return &timer
}

type TlvData struct {
	Type  uint16
	Value string
}

// types
const (
	ERROR   uint16 = 0
	RESPOND uint16 = 1
	PING    uint16 = 2
	UPDATE  uint16 = 3
	CLOSE   uint16 = 4
	RELOAD  uint16 = 5
	KILL    uint16 = 6
)

func (tlv TlvData) toBytes() []byte {
	valueInByte := []byte(tlv.Value)
	length := uint64(len(valueInByte))

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

func client(sendTlv TlvData, socketAddress string) (TlvData, error) {
	var recv TlvData
	conn, err := net.DialUnix("unix", nil, &net.UnixAddr{Name: socketAddress, Net: "unix"})
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
	Shell           []string `json:"shell"`
	timeoutDuration time.Duration
	Timeout         string   `json:"timeout"`
	OpenCommand     string   `json:"openCommand"`
	UpdateCommand   string   `json:"updateCommand"`
	CloseCommand    string   `json:"closeCommand"`
	Options         []string `json:"options"`
}

func loadConfig(configPath string) (Config, error) {
	config := Config{Shell: slices.Clone(DEFAULT_SHELL)}

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

func (config Config) String() string {
	return strings.Join([]string{
		"config:",
		fmt.Sprintf("shell: %v", config.Shell),
		fmt.Sprintf("timeout: %v", config.timeoutDuration.String()),
		fmt.Sprintf("open command: %v", config.OpenCommand),
		fmt.Sprintf("update command: %v", config.UpdateCommand),
		fmt.Sprintf("close command: %v", config.CloseCommand),
		fmt.Sprintf("options: %v", config.Options),
	}, "\n")
}

func execute(commands []string) {
	exec.Command(commands[0], commands[1:]...).Run()
}

func openDaemon(socketAddress string, configPath string) error {
	if configPath == "" {
		configDir, err := os.UserConfigDir()
		if err != nil {
			return err
		}
		configPath = configDir + "/snackdaemon/snackdaemon.json"
	}
	config, err := loadConfig(configPath)
	if err != nil {
		return fmt.Errorf("error: invalid config file (%v)", err)
	}

	fmt.Printf("%+v\n", config)
	fmt.Println("----------")

	listener, err := net.ListenUnix("unix", &net.UnixAddr{Name: socketAddress, Net: "unix"})
	if err != nil {
		return fmt.Errorf("error: can not listen to address `%v` (%v)", socketAddress, err)
	}
	defer listener.Close()

	timer := NewTimer(0, func() {})
	for {
		shouldContinue, message, err := handleConnection(listener, timer, &config, &configPath)
		if err != nil {
			log.Println(err)
			continue
		}
		fmt.Println(message)
		if !shouldContinue {
			break
		}
	}
	return nil
}

func handleConnection(listener net.Listener, timer *Timer, config *Config, configPath *string) (bool, string, error) {
	conn, err := listener.Accept()
	if err != nil {
		return false, "", fmt.Errorf("Can not accept this (%v)", err)
	}
	defer conn.Close()

	tlv, err := recvTlv(conn)
	if err != nil {
		return false, "", err
	}

	switch tlv.Type {
	case PING:
		conn.Write(TlvData{Type: RESPOND, Value: "pong"}.toBytes())
		return true, "ping", nil
	case KILL:
		conn.Write(TlvData{Type: RESPOND, Value: "ok"}.toBytes())
		return false, "kill", nil
	case CLOSE:
		if !timer.Stopped() {
			timer.Cancel()
			execute(append(slices.Clone(config.Shell), config.CloseCommand))
		}

		conn.Write(TlvData{Type: RESPOND, Value: ""}.toBytes())
		return true, "close", nil
	case UPDATE:
		index := slices.Index(config.Options, tlv.Value)
		if index == -1 {
			conn.Write(TlvData{Type: RESPOND, Value: "no such option"}.toBytes())
			return true, fmt.Sprintf("update: %s (no such option)", tlv.Value), nil
		}
		if timer.Stopped() {
			execute(append(slices.Clone(config.Shell), config.OpenCommand))
		}
		execute(append(slices.Clone(config.Shell), fmt.Sprintf(config.UpdateCommand, index)))
		timer.Cancel()
		timer = NewTimer(config.timeoutDuration, func() {
			execute(append(slices.Clone(config.Shell), config.CloseCommand))
		})

		conn.Write(TlvData{Type: RESPOND, Value: ""}.toBytes())
		return true, fmt.Sprintf("update: %s (index: %d)", tlv.Value, index), nil
	case RELOAD:
		newConfigPath := tlv.Value
		if newConfigPath == "" {
			newConfigPath = *configPath
		}
		newConfig, err := loadConfig(newConfigPath)
		if err != nil {
			conn.Write(TlvData{Type: RESPOND, Value: "failed to reload"}.toBytes())
			return false, "", fmt.Errorf("reload: failed to reload with \"%v\"", newConfigPath)
		}
		*config = newConfig
		*configPath = newConfigPath

		conn.Write(TlvData{Type: RESPOND, Value: "ok"}.toBytes())
		return true, strings.Join([]string{
			fmt.Sprintf("reload: reload with `%v`", newConfigPath),
			"----------",
			fmt.Sprintf("config: %+v", config),
			"----------",
		}, "\n"), nil
	}
	return false, "", fmt.Errorf("Unknown message: %v", tlv)
}

type Args struct {
	command       string
	help          bool
	socketAddress string
	configPath    string
	updateOption  string
}

// tags: --help -h --socket -s --config -c
// commands: daemon kill ping close reload update help
func loadArgs() (Args, error) {
	args := Args{
		help:          false,
		socketAddress: DEFAULT_SOCKET_ADDRESS,
		configPath:    "",
	}
	argsSetted := struct {
		command       bool
		socketAddress bool
		configPath    bool
	}{
		command:       false,
		socketAddress: false,
		configPath:    false,
	}
	for i := 1; i < len(os.Args); i++ {
		switch os.Args[i] {
		case "--help", "-h":
			if args.help {
				return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
			}

			args.help = true
		case "--socket", "-s":
			i++
			if argsSetted.socketAddress || i >= len(os.Args) {
				return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
			}

			argsSetted.socketAddress = true
			if os.Args[i] == "@" && i+1 < len(os.Args) {
				i++
				args.socketAddress = "\x00" + os.Args[i]
			} else {
				args.socketAddress = os.Args[i]
			}
		case "--config", "-c":
			i++
			if argsSetted.configPath || i >= len(os.Args) {
				return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
			}

			argsSetted.configPath = true
			args.configPath = os.Args[i]
		case "daemon", "kill", "ping", "reload", "close", "generate-config", "help":
			if argsSetted.command {
				return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
			}

			argsSetted.command = true
			args.command = os.Args[i]
		case "update":
			i++
			if args.command != "" || i >= len(os.Args) {
				return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
			}

			argsSetted.command = true
			args.command = "update"
			args.updateOption = os.Args[i]
		default:
			return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
		}
	}

	if !argsSetted.command {
		return Args{}, fmt.Errorf("invalid arguments, try `snackdaemon help` to get help.")
	}

	return args, nil
}

func help(command string) string {
	// TODO: update with new flags
	switch command {
	case "daemon":
		// TODO: add daemon help
		return "The help for daemon"
	case "kill":
		// TODO: add kill help
		return "The help for kill"
	case "ping":
		// TODO: add ping help
		return "The help for ping"
	case "reload":
		// TODO: add reload help
		return "The help for reload"
	case "close":
		// TODO: add close help
		return "The help for close"
	case "update":
		// TODO: add update help
		return "The help for update"
	case "generate-config":
		// TODO: add generate-config help
		return "The help for generate-config"
	case "help":
		return strings.Join([]string{
			"usage: snackdaemon <command> [<args>]",
			"commands:",
			"    help            Print help",
			"    daemon          Start the daemon",
			"    reload          reload the config",
			"    kill            Send \"kill\" to the daemon",
			"    ping            Ping the daemon to check connectivity",
			"    update <arg>    Update with <arg>'s index in \"options\" in config file",
			"    close           Trigger the \"closeCommand\" in config file and end timer",
			"",
			"Visit 'https://github.com/Shiphan/snackdaemon' for more information or bug report.",
			"usage: snackdaemon help",
			"Print help",
		}, "\n")
	default:
		return "invalid arguments, try `snackdaemon help` to get help."
	}
}

func main() {
	args, err := loadArgs()
	if err != nil {
		fmt.Println(err)
		return
	}

	if args.help {
		fmt.Println(help(args.command))
		return
	}

	switch args.command {
	case "daemon":
		err := openDaemon(args.socketAddress, args.configPath)
		if err != nil {
			log.Fatal(err)
		}
	case "kill":
		recv, err := client(TlvData{KILL, ""}, args.socketAddress)
		if err != nil {
			log.Fatalf("Unable to connect to daemon. (%v)", err)
		}
		fmt.Println(recv.Value)
	case "ping":
		start := time.Now()
		recv, err := client(TlvData{PING, ""}, args.socketAddress)
		if err != nil {
			log.Fatalf("Unable to connect to daemon. (%v)", err)
		}
		end := time.Now()
		fmt.Printf("%v (latency: %s)\n", recv.Value, end.Sub(start).String())
	case "reload":
		absConfigPath, err := filepath.Abs(args.configPath)
		if err != nil {
			log.Fatal(err)
		}
		recv, err := client(TlvData{RELOAD, absConfigPath}, args.socketAddress)
		if err != nil {
			log.Fatalf("Unable to connect to daemon. (%v)", err)
		}
		fmt.Println(recv.Value)
	case "close":
		recv, err := client(TlvData{CLOSE, ""}, args.socketAddress)
		if err != nil {
			log.Fatalf("Unable to connect to daemon. (%v)", err)
		}
		fmt.Println(recv.Value)
	case "update":
		recv, err := client(TlvData{UPDATE, args.updateOption}, args.socketAddress)
		if err != nil {
			log.Fatalf("Unable to connect to daemon. (%v)", err)
		}
		fmt.Println(recv.Value)
	case "generate-config":
		b, err := json.MarshalIndent(Config{Timeout: "2s", OpenCommand: "eww open snackbar", UpdateCommand: "eww update snackbarIndex=%d", CloseCommand: "eww close snackbar", Options: []string{"volume", "player", "screenbrightness", "powerprofiles"}}, "", "\t")
		if err != nil {
			fmt.Println(err)
		}
		fmt.Println(string(b))
	case "help":
		fmt.Println(help("help"))
	default:
		fmt.Println(help(""))
	}
}
