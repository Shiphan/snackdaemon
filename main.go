package main

import (
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"os"
	"os/exec"
	"slices"
	"strings"
	"time"
)

const SOCKET_PATH string = "/tmp/snackdaemon.sock"

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
		return tlv, errors.New("This is a invalid TLV data")
	}

	length := binary.BigEndian.Uint64(bytes[2:])
	if length+11 != uint64(len(bytes)) {
		return tlv, errors.New("This is a invalid TLV data")
	}

	value := string(bytes[10 : len(bytes)-1])

	if strings.Index(value, string(rune(0))) != -1 {
		return tlv, errors.New("This is a invalid TLV data")
	}

	tlv.Type = binary.BigEndian.Uint16(bytes[0:])
	tlv.Value = value

	return tlv, nil
}

func printHelp() {
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

func client(sendTlv TlvData) (TlvData, error) {
	var recv TlvData
	conn, err := net.Dial("unix", SOCKET_PATH)
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

func openDaemon() {
	homedir, err := os.UserHomeDir()
	if err != nil {
		fmt.Println("Can not get your home directory.")
		return
	}

	configPath := homedir + "/.config/snackdaemon/snackdaemon.json"
	config, err := loadConfig(configPath)
	if err != nil {
		printInvalidConfig()
		return
	}

	printConfig(config)
	fmt.Println("----------")

	recv, err := client(TlvData{KILL, ""})
	if err == nil {
		fmt.Printf("sent kill to the old daemon and the response is: %v\n", recv.Value)
	}

	os.Remove(SOCKET_PATH)
	listener, err := net.Listen("unix", SOCKET_PATH)
	if err != nil {
		fmt.Printf("Can not listen to \"%s\"", SOCKET_PATH)
	}
	defer os.Remove(SOCKET_PATH)
	defer listener.Close()

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
				exec.Command("bash", "-c", config.CloseCommand).Run()
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
				exec.Command("bash", "-c", config.OpenCommand).Run()
			}

			exec.Command("bash", "-c", fmt.Sprintf(config.UpdateCommand, index)).Run()

			fmt.Printf("update: %s (index: %d)\n", tlv.Value, index)

			timer.cancel()
			timer = NewTimer(config.timeoutDuration, func() { exec.Command("bash", "-c", config.CloseCommand).Run() }, true)

			conn.Write(TlvData{Type: RESPOND, Value: ""}.toBytes())
		case RELOAD:
			newConfig, err := loadConfig(configPath)
			if err != nil {
				fmt.Println("reload: failed to reload")
				conn.Write(TlvData{Type: RESPOND, Value: "failed to reload"}.toBytes())
				continue
			}

			config = newConfig
			conn.Write(TlvData{Type: RESPOND, Value: "ok"}.toBytes())

			fmt.Println("----------")
			printConfig(config)
			fmt.Println("----------")
		default:
			fmt.Printf("Unknown message: %v\n", tlv)
		}
	}
}

func main() {
	switch len(os.Args) {
	case 1:
		printHelp()
	case 2:
		switch os.Args[1] {
		case "daemon":
			openDaemon()
		case "kill":
			recv, err := client(TlvData{KILL, ""})
			if err != nil {
				fmt.Println("Unable to connect to daemon.")
				break
			}
			fmt.Println(recv.Value)
		case "ping":
			start := time.Now()
			recv, err := client(TlvData{PING, ""})
			if err != nil {
				fmt.Println("Unable to connect to daemon.")
				break
			}
			end := time.Now()
			fmt.Printf("%v (latency: %s)\n", recv.Value, end.Sub(start).String())
		case "close":
			recv, err := client(TlvData{CLOSE, ""})
			if err != nil {
				fmt.Println("Unable to connect to daemon.")
				break
			}
			fmt.Println(recv.Value)
		case "reload":
			recv, err := client(TlvData{RELOAD, ""})
			if err != nil {
				fmt.Println("Unable to connect to daemon.")
				break
			}
			fmt.Println(recv.Value)
		case "help":
			printHelp()
		default:
			printInvalidArgs()
		}
	case 3:
		switch os.Args[1] {
		case "update":
			recv, err := client(TlvData{UPDATE, os.Args[2]})
			if err != nil {
				fmt.Println("Unable to connect to daemon.")
				break
			}
			fmt.Println(recv.Value)
		default:
			printInvalidArgs()
		}
	default:
		printInvalidArgs()
	}
}
