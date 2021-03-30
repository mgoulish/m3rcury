package main

import (
         "fmt"
         "os"

         m3 "m3rcury"
       )


var fp = fmt.Fprintf




func main ( ) {

  m3rcury_output := m3.Start_M3rcury ( )

  for {
    msg := <- m3rcury_output
    fp ( os.Stdout, "received msg of type %s : |%s|\n", msg.Type, msg.Data["msg"] )
  }
}



