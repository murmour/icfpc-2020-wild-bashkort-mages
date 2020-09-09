
open Printf

module HT = Hashtbl
module BF = Buffer
module S = String
module B = Bytes
module L = List
module A = Array


type expr =
  | Int of Z.t
  | Sym of int
  | Ap of expr * expr
  | Cons | Cons1 of expr | Cons2 of expr * expr
  | Car
  | Cdr
  | S | S1 of expr | S2 of expr * expr
  | B | B1 of expr | B2 of expr * expr
  | C | C1 of expr | C2 of expr * expr
  | I
  | Nil
  | IsNil
  | Neg
  | Add | Add1 of expr
  | Mul | Mul1 of expr
  | Div | Div1 of expr
  | Lt | Lt1 of expr
  | Eq | Eq1 of expr
  | T | T1 of expr
  | F | F1
  | Lazy of expr Lazy.t

and stmt = int * expr

and protocol = stmt list


(* Parsing
   -------------------------------------------------------------------------- *)

let rec drop_last_nil: expr -> expr = function
  | Cons2 (h, t) ->
      if t = Nil then h else Cons2 (h, drop_last_nil t)
  | Nil -> Nil
  | _ -> assert false


let parse_expr (s: string) : expr =
  let s = s ^ "\n" in
  let i = ref 0 in

  let rec expr () : expr =
    match s.[!i] with
      | ' ' -> incr i; expr ()
      | '-' | '0'..'9' -> Int (int ())
      | ':' -> incr i; Sym (Z.to_int (int ()))
      | '[' -> incr i; list1 ()
      | '(' -> incr i; drop_last_nil (list2 ())
      | 'a'..'z' ->
          begin match ident () with
            | "ap" -> let a = expr () in let b = expr () in Ap (a, b)
            | "cons" -> Cons
            | "car" -> Car
            | "cdr" -> Cdr
            | "mul" -> Mul
            | "div" -> Div
            | "lt" -> Lt
            | "nil" -> Nil
            | "isnil" -> IsNil
            | "neg" -> Neg
            | "eq" -> Eq
            | "add" -> Add
            | "s" -> S
            | "b" -> B
            | "c" -> C
            | "i" -> I
            | "t" -> T
            | "f" -> F
            | etc ->
                failwith (sprintf "unknown id: %s" etc)
          end
      | _ ->
          assert false

  and int () : Z.t =
    let b = BF.create 20 in
    while match s.[!i] with '-' | '0'..'9' -> true | _ -> false do
      BF.add_char b s.[!i];
      incr i;
    done;
    Z.of_string (BF.contents b)

  and ident () : string =
    let b = BF.create 20 in
    while match s.[!i] with 'a'..'z' -> true | _ -> false do
      BF.add_char b s.[!i];
      incr i;
    done;
    BF.contents b

  and list1 () =
    match s.[!i] with
      | ' ' -> incr i; list1 ()
      | ']' -> incr i; Nil
      | _ -> let e = expr () in Cons2 (e, list1 ())

  and list2 () =
    match s.[!i] with
      | ' ' -> incr i; list2 ()
      | ')' -> incr i; Nil
      | _ -> let e = expr () in Cons2 (e, list2 ())
  in

  expr ()


let parse_stmt (s: string) : stmt =
  match S.split_on_char '=' s with
    | [ lhs; rhs ] ->
        let sym = match S.trim lhs with
          | "galaxy" -> 0
          | s -> int_of_string (S.sub s 1 (S.length s - 1))
        in
        (sym, parse_expr rhs)
    | _ ->
        assert false


let parse_protocol (s: string) : protocol =
  s |> S.split_on_char '\n' |> L.filter ((<>) "") |> L.map parse_stmt


(* Loading galaxy
   -------------------------------------------------------------------------- *)

type tprotocol = expr array


let tabulate (p: protocol) : tprotocol =
  let i = ref 0 in
  let h = HT.create 1000 in
  HT.add h 0 0;
  p |> L.iter (fun (lhs, _) -> if lhs <> 0 then (incr i; HT.add h lhs !i));
  let rec map: expr -> expr = function
    | Sym i -> Sym (HT.find h i)
    | Ap (e1, e2) -> Ap (map e1, map e2)
    | etc -> etc
  in
  let arr = A.make (!i+1) Nil in
  p |> L.iter (fun (lhs, rhs) ->
    let lhs = HT.find h lhs in
    arr.(lhs) <- map rhs
  );
  arr


let vars: tprotocol =
  let ch = open_in "galaxy.txt" in
  let s = really_input_string ch (in_channel_length ch) in
  close_in ch;
  s |> parse_protocol |> tabulate


let galaxy = Sym 0


(* Evaluating
   -------------------------------------------------------------------------- *)

let rec eval: expr -> expr = function
  | Sym i -> vars.(i)
  | Lazy e -> Lazy.force e
  | Ap (f, x) -> eval_ap f x
  | etc -> etc

and eval_lazy: expr -> expr = function
  | Ap (f, x) -> Lazy (lazy (eval_ap f x))
  | etc -> etc

and eval_int (e: expr) : Z.t =
  match eval e with
    | Int i -> i
    | _ -> assert false

and eval_ap (f: expr) (x: expr) : expr =
  let f = eval f in
  let b = x and c = x in (* for clarity *)
  match f with
    | Nil -> T
    | IsNil -> if eval x = Nil then T else F
    | Cons -> Cons1 x
    | Cons1 a -> Cons2 (a, b)
    | Cons2 (a, b) -> eval_ap (eval_ap c a) b
    | Car -> (match eval x with Cons2 (a, _) -> eval a | _ -> assert false)
    | Cdr -> (match eval x with Cons2 (_, b) -> eval b | _ -> assert false)
    | I -> eval x
    | Neg -> Int (Z.neg (eval_int x))
    | T -> T1 x
    | T1 a -> eval a
    | F -> F1
    | F1 -> eval b
    | Add -> Add1 x
    | Add1 a -> Int Z.(eval_int a + eval_int b)
    | Mul -> Mul1 x
    | Mul1 a -> Int Z.(eval_int a * eval_int b)
    | Div -> Div1 x
    | Div1 a -> Int Z.(eval_int a / eval_int b)
    | Lt -> Lt1 x
    | Lt1 a -> if Z.(eval_int a < eval_int b) then T else F
    | Eq -> Eq1 x
    | Eq1 a -> if eval a = eval b then T else F
    | S -> S1 x
    | S1 a -> S2 (a, b)
    | S2 (a, b) ->
        let c = eval_lazy c in
        eval_ap (eval_ap a c) (Lazy (lazy (eval_ap (eval_lazy b) c)))
    | B -> B1 x
    | B1 a -> B2 (a, b)
    | B2 (a, b) ->
        eval_ap a (Lazy (lazy (eval_ap b (eval_lazy c))))
    | C -> C1 x
    | C1 a -> C2 (a, b)
    | C2 (a, b) ->
        eval_ap (eval_ap a (eval_lazy c)) (eval_lazy b)
    | Ap _ | Int _ | Sym _ | Lazy _ -> assert false


(* Evaluating globals
   -------------------------------------------------------------------------- *)

let () = vars |> A.iteri (fun i v -> vars.(i) <- eval v)


(* Sending
   -------------------------------------------------------------------------- *)

let server_url = "https://api.pegovka.space"
let send_url = server_url ^ "/aliens/send"


let rec modem (e: expr) : expr =
  match eval e with
    | (Nil | Int _) as atom ->
        atom
    | Cons2 (car, cdr) ->
        Cons2 (modem car, modem cdr)
    | _ ->
        assert false


let rec modulate: expr -> string = function
  | Nil ->
      "00"
  | Cons2 (car, cdr) ->
      "11" ^ modulate car ^ modulate cdr
  | Int n ->
      let sign = if Z.(n < zero) then "10" else "01" in
      let n = Z.abs n in
      let nb = Z.numbits n in
      let mantissa = (nb+3)/4 in
      let bit_count = mantissa*4 in
      let bits = B.make bit_count '0' in
      for i = 0 to bit_count - 1 do
        if Z.testbit n i then B.set bits (bit_count-i-1) '1'
      done;
      sign ^ S.make mantissa '1' ^ "0" ^ (B.to_string bits)
  | _ ->
      assert false


let demodulate (s: string) : expr =
  let i = ref 0 in

  let rec parse_list () : expr =
    i := !i + 2;
    match (s.[!i-2], s.[!i-1]) with
      | ('0', '0') ->
          Nil
      | ('1', '1') ->
          let car = parse_list () in
          let cdr = parse_list () in
          Cons2 (car, cdr)
      | ('0', '1') ->
          Int (parse_int ())
      | ('1', '0') ->
          Int (Z.neg (parse_int ()))
      | _ ->
          assert false

  and parse_int () : Z.t =
    let mantissa =
      let k = ref 0 in
      while s.[!i] <> '0' do incr i; incr k done;
      incr i;
      !k
    in
    if mantissa = 0 then
      Z.zero
    else
      let bitcount = mantissa*4 in
      let bits = S.sub s !i (mantissa*4) in
      i := !i + bitcount;
      Z.of_string ("0b" ^ bits)
  in

  parse_list ()


let curl_initialized = ref false

let http_post ~url ~data : int * string =
  if not !curl_initialized then (
    Curl.global_init Curl.CURLINIT_GLOBALALL;
    curl_initialized := true
  );
  let b = BF.create 100 in
  let c = Curl.init () in
  Curl.set_timeout c 100; (* seconds *)
  Curl.set_sslverifypeer c false;
  Curl.set_sslverifyhost c Curl.SSLVERIFYHOST_EXISTENCE;
  Curl.set_writefunction c (fun s -> BF.add_string b s; S.length s);
  Curl.set_tcpnodelay c true;
  Curl.set_verbose c false;
  Curl.set_url c url;
  Curl.set_post c true;
  Curl.set_postfields c data;
  Curl.set_postfieldsize c (S.length data);
  Curl.perform c;
  let code = Curl.get_responsecode c in
  Curl.cleanup c;
  (code, BF.contents b)


let send (data: expr) : expr =
  let data = data |> modem |> modulate in
  match http_post ~url:send_url ~data with
    | (200, body) ->
        demodulate body
    | (code, body) ->
        failwith (sprintf "Unexpected HTTP response (%d): %s" code body)


(* Interacting (Galaxy Pad backend)
   -------------------------------------------------------------------------- *)

let rec make_list: expr list -> expr = function
  | [] -> Nil
  | x :: xs -> Cons2 (x, make_list xs)


let rec extract_list (e: expr) : expr list =
  match eval e with
    | Nil -> []
    | Cons2 (car, cdr) ->
        eval car :: extract_list cdr
    | _ ->
        assert false


let rec interact protocol state vector : expr =
  let response = eval (Ap (Ap (protocol, state), vector)) in
  match extract_list response with
    | [ flag; state; data ] ->
        begin match flag with
          | Int i when Z.(i = zero) ->
              make_list [ modem state; data ]
          | Int i when Z.(i = one) ->
              interact protocol (modem state) (send data)
          | _ ->
              assert false
        end
    | _ ->
        assert false


let extract_point: expr -> int * int = function
  | Cons2 (Int x, Int y) ->
      (Z.to_int x, Z.to_int y)
  | _ ->
      assert false


let draw_image (i: int) (e: expr) : unit =
  let pts = e |> extract_list |> L.map extract_point in
  let (min_x, min_y, len_x, len_y) =
    if pts = [] then
      (0, 0, 0, 0)
    else
      let min_x = ref max_int in
      let min_y = ref max_int in
      let max_x = ref min_int in
      let max_y = ref min_int in
      pts |> L.iter (fun (x, y) ->
        min_x := min !min_x x;
        min_y := min !min_y y;
        max_x := max !max_x x;
        max_y := max !max_y y;
      );
      let len_x = !max_x - !min_x + 1 in
      let len_y = !max_y - !min_y + 1 in
      (!min_x, !min_y, len_x, len_y)
  in
  let mat = A.init len_y (fun _ -> B.make len_x '.') in
  pts |> L.iter (fun (x, y) ->
    B.set mat.(y - min_y) (x - min_x) '#');
  let ch = open_out_bin (sprintf "image_%d.log" i) in
  fprintf ch "@(%d, %d)\n" min_x min_y;
  mat |> A.iter (fun s ->
    fprintf ch "%s\n" (B.to_string s));
  close_out ch


let rec print_state: expr -> string = function
  | Nil -> "nil"
  | Int i -> Z.to_string i
  | (Cons2 _) as cons ->
      let l = ref [] in
      let rec iter = function
        | Cons2 (car, cdr) ->
            l := print_state car :: !l;
            iter cdr
        | Nil ->
            `List1
        | etc ->
            l := print_state etc :: !l;
            `List2
      in
      begin match iter cons with
        | `List1 ->
            "[" ^ S.concat " " (L.rev !l) ^ "]"
        | `List2 ->
            "(" ^ S.concat " " (L.rev !l) ^ ")"
      end
  | _ ->
      assert false


(* REPL *)
let () =
  while true do
    let line1 = Scanf.scanf "%s@\n" (fun x -> x) in
    let line2 = Scanf.scanf "%s@\n" (fun x -> x) in
    try
      let input = parse_expr line1 in
      let state = parse_expr line2 in
      let result = interact galaxy state input in
      match extract_list result with
        | [ state; images ] ->
            let images = images |> modem |> extract_list in
            images |> L.iteri draw_image;
            printf "%d\n%s\n%!" (L.length images) (print_state state);
        | _ ->
            assert false
    with exc ->
      eprintf "%s\n%!" (Printexc.to_string exc);
      printf "-1\n%!"
  done
