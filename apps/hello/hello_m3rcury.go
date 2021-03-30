package main

import (
         "fmt"
         "os"
         "time"

         m3 "m3rcury"
       )


var fp = fmt.Fprintf





func main ( ) {

  m3rcury_input, m3rcury_output := m3.Start_M3rcury ( )

  m3rcury_input <- m3.Message { Type: "command",
                                Data: map[string]interface{} { "log" : "./log"} }

  for { 
    msg := <- m3rcury_output
    fp ( os.Stdout, "Main received msg: |%v|\n", msg )

    time.Sleep ( 3 * time.Second )

    switch msg.Data["msg"] {
      case "thanks" :
        m3rcury_input <- m3.Message { Type: "command",
                                     Data: map[string]interface{} { "log" : "you're welcome"} }
    }
  }
}



