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

  m3rcury_input <- m3.Message { Type: "box",
                                Data: map[string]interface{} { "name" : "Brontonomicon"} }

  m3rcury_input <- m3.Message { Type: "box",
                                Data: map[string]interface{} { "name" : "Colossus-Guardian"} }

  m3rcury_input <- m3.Message { Type: "network",
                                Data: map[string]interface{} { "name"              : "fastwalker",
                                                               "Brontonomicon"     : "10.10.10.1",
                                                               "Colossus-Guardian" : "10.10.10.2" } }

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



