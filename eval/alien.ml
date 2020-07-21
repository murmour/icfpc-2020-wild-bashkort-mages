
open Printf


let post ~url ~data : int * string =
  let b = Buffer.create 100 in
  let c = Curl.init () in
  Curl.set_timeout c 100; (* seconds *)
  Curl.set_sslverifypeer c false;
  Curl.set_sslverifyhost c Curl.SSLVERIFYHOST_EXISTENCE;
  Curl.set_writefunction c (fun s -> Buffer.add_string b s; String.length s);
  Curl.set_tcpnodelay c true;
  Curl.set_verbose c false;
  Curl.set_url c url;
  Curl.set_post c true;
  Curl.set_postfields c data;
  Curl.set_postfieldsize c (String.length data);
  Curl.perform c;
  let code = Curl.get_responsecode c in
  Curl.cleanup c;
  (code, Buffer.contents b)


let () =
  let url = Sys.argv.(1) in
  let ch = open_in "send.txt" in
  let data = String.trim (really_input_string ch (in_channel_length ch)) in
  Curl.global_init Curl.CURLINIT_GLOBALALL;
  match post ~url ~data with
    | (200, body) ->
        let h = open_out "alien.txt" in
        output_string h body
    | (code, body) ->
        printf "Unexpected server response:\n";
        printf "HTTP code: %d\n" code;
        printf "Response body: %s\n" body;
        exit 2
