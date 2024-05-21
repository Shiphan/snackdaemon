package main

import (
	"fmt"
	"os"
	"syscall"
)

func main() {
	switch {
	case len(os.Args) == 2 && os.Args[1] == "open":
		fd := syscall.ShmOpen()
	case len(os.Args) == 2 && os.Args[1] == "run":
	case len(os.Args) == 2 && os.Args[1] == "kill":

	default:
		fmt.Println("invalid args")
	}

}
