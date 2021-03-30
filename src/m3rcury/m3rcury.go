package m3rcury

import ( 
         "fmt"
         "os"
       )

var fp = fmt.Fprintf



type Message struct {
  Type string
  Data map [ string ] interface{}
}


type Message_Channel chan Message



type M3rcury struct {
  input  Message_Channel
  output Message_Channel
}



func Start_M3rcury ( ) ( in, out Message_Channel ) {
  in  = make ( Message_Channel, 5 )
  out = make ( Message_Channel, 5 )
  m3 := & M3rcury { output : out,
                    input  : in   }
  go m3.listen (  )
  return in, out
}





func ( m3 * M3rcury ) listen ( ) {
  for { 
    msg := <- m3.input
    fp ( os.Stdout, "☿: received message: |%v|\n", msg )
    m3.output <- Message { Type: "info",
                           Data: map[string]interface{} { "msg" : "thanks"} }
  }
}





