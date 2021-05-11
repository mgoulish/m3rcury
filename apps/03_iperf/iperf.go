package main

import (
         "fmt"
         "os"
         "time"

         m3 "m3rcury"
       )


var fp = fmt.Fprintf





func main ( ) {

  m3rcury_input, m3rcury_output := m3.Start_M3rcury ( "Brontonomicon", "./log" )

  m3rcury_input <- m3.Message { Type: "iperf",
                                Data: map[string]interface{} { "mode" : "server"} }

  for { 
    msg := <- m3rcury_output
    fp ( os.Stdout, "Main received msg: |%v|\n", msg )


    switch msg.Type {
      case "ping" :
         time.Sleep ( 3 * time.Second )
         m3rcury_input <- m3.Message { Type: "pong" }
    }
  }
}



