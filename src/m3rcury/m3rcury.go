package m3rcury

import ( 
         "fmt"
         "time"
       )

var fp = fmt.Fprintf



type Message struct {
  Type string
  Data map [ string ] interface{}
}


type Message_Channel chan Message



type M3rcury struct {
  output_channel Message_Channel
}



func Start_M3rcury ( ) ( Message_Channel ) {
  out := make ( Message_Channel, 5 )
  m3 := & M3rcury { output_channel : out }
  go m3.run (  )
  return out
}




func ( m3 * M3rcury ) run ( ) {

  for {
    m3.output_channel <- Message { Type: "info",
                                   Data: map[string]interface{} { "msg" : "M3rcury is running!"} }
    time.Sleep ( 5 * time.Second )
  }
}





