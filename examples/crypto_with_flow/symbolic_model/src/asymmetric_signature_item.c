#include "asymmetric_signature_item.h"

#include "item_constraints.h"
#include "key_item.h"
#include "principal_ids.h"
#include "serialization.h"
#include "deserialization.h"

#include <string.h>

bool is_asymmetric_signature(struct item *item)
  //@ requires [?f]world(?pub) &*& item(item, ?i, pub);
  /*@ ensures  [f]world(pub) &*& item(item, i, pub) &*&
               result ? i == asymmetric_signature_item(_, _, _, _) : true; @*/
{
  //@ open [f]world(pub);
  //@ open item(item, i, pub);
  //@ open [_]item_constraints(i, ?cs, pub);
  return item_tag(item->content, item->size) == TAG_ASYMMETRIC_SIG;
  //@ close item(item, i, pub);
  //@ close [f]world(pub);
}

void check_is_asymmetric_signature(struct item *item)
  //@ requires [?f]world(?pub) &*& item(item, ?i, pub);
  /*@ ensures  [f]world(pub) &*& item(item, i, pub) &*&
               i == asymmetric_signature_item(_, _, _, _); @*/
{
  if (!is_asymmetric_signature(item))
    abort_crypto_lib("Presented item is not an asymmetric signature item");
}

/*@
lemma void info_for_asymmetric_signature_item(item key, item sig)
  requires [_]info_for_item(key, ?info1) &*&
           key == private_key_item(?p, ?c) &*&
           [_]info_for_item(sig, ?info2) &*&
           sig == asymmetric_signature_item(p, c, _, _);
  ensures  info1 == info2;
{
  open [_]info_for_item(key, info1);
  open [_]info_for_item(sig, info2);
}
@*/

int asym_sig_havege_random_stub(void *havege_state, char *output, size_t len)
  /*@ requires [?f]havege_state_initialized(havege_state) &*&
               random_request(?principal, ?info, ?key_request) &*&
               principal(principal, ?count) &*&
               chars(output, len, _) &*& len >= MIN_RANDOM_SIZE;
  @*/
  /*@ ensures  [f]havege_state_initialized(havege_state) &*&
               principal(principal, count + 1) &*&
               result == 0 ?
                 cryptogram(output, len, ?cs, ?cg) &*&
                 info == cg_info(cg) &*&
                 key_request ?
                   cg == cg_symmetric_key(principal, count + 1)
                 :
                   cg == cg_nonce(principal, count + 1)
               :
                 chars(output, len, _);
  @*/
{
  return havege_random(havege_state, output, len);
}

struct item *asymmetric_signature(struct item *key, struct item *payload)
  /*@ requires [?f]world(?pub) &*&
               principal(?principal1, ?count1) &*&
               item(payload, ?pay, pub) &*& item(key, ?k, pub) &*&
               k == private_key_item(?principal2, ?count2); @*/
  /*@ ensures  [f]world(pub) &*&
               principal(principal1, count1 + 1) &*&
               item(payload, pay, pub) &*& item(key, k, pub) &*&
               item(result, ?sig, pub) &*&
               col ? true :
                 sig == asymmetric_signature_item(principal2, count2,
                                                  some(pay), ?ent); @*/
{
  debug_print("ASYM SIGNING:\n");
  print_item(payload);

  struct item* result;
  result = malloc(sizeof(struct item));
  if (result == 0) abort_crypto_lib("Malloc failed");

  debug_print("signing item\n");
  print_item(payload);
  print_item(key);

  {
    pk_context context;
    unsigned int olen;
    char* output;

    // Key
    //@ close pk_context(&context);
    //@ open [f]world(pub);
    pk_init(&context);
    //@ close [f]world(pub);
    set_private_key(&context, key);
    //@ open [f]world(pub);
    /*@ assert pk_context_with_key(&context, pk_private,
                                   ?principal, ?count, RSA_BIT_KEY_SIZE); @*/
    //@ assert col || principal == principal2;
    //@ assert col || count == count2;

    // Payload
    //@ open item(payload, pay, pub);
    //@ open [_]item_constraints(pay, ?pay_cs, pub);
    //@ assert payload->content |-> ?p_cont &*& payload->size |-> ?p_size;
    if (payload->size > RSA_KEY_SIZE)
      abort_crypto_lib("Assymetric signing failed: incorrect size");
    output = malloc(RSA_KEY_SIZE);
    if (output == 0) abort_crypto_lib("Malloc failed");

    void *random_state = nonces_expose_state();
    //@ close random_state_predicate(havege_state_initialized);
    /*@ produce_function_pointer_chunk random_function(
                      asym_sig_havege_random_stub)
                     (havege_state_initialized)(state, out, len) { call(); } @*/
    if(pk_sign(&context, POLARSSL_MD_NONE,
               payload->content, (unsigned int) payload->size,
               output, &olen,
               asym_sig_havege_random_stub, random_state) != 0)
      abort_crypto_lib("Signing failed");
    //@ open cryptogram(output, ?sig_length, ?sig_cs, ?sig_cg);
    //@ assert sig_cg == cg_asym_signature(principal, count, pay_cs, ?ent);
    //@ assert u_integer(&olen, sig_length);
    //@ assert sig_length > 0 &*& sig_length <= RSA_KEY_SIZE;
    nonces_hide_state(random_state);
    //@ pk_release_context_with_key(&context);
    pk_free(&context);
    //@ open pk_context(&context);
    //@ close [f]world(pub);
    debug_print("signed item\n");
    print_buffer(output, (int) olen);

    // Create item
    if (olen < MINIMAL_STRING_SIZE)
      abort_crypto_lib("Assymetric signing failed: output to small");
    result->size = TAG_LENGTH + (int) olen;
    result->content = malloc(result->size);
    if (result->content == 0) {abort_crypto_lib("Malloc failed");}
    write_tag(result->content, TAG_ASYMMETRIC_SIG);
    //@ assert result->content |-> ?cont &*& result->size |-> ?size;
    //@ assert chars(cont, TAG_LENGTH, ?tag_cs);
    //@ assert tag_cs == full_tag(TAG_ASYMMETRIC_SIG);
    //@ public_chars(cont, TAG_LENGTH);
    //@ chars_to_secret_crypto_chars(cont, TAG_LENGTH);
    memcpy(result->content + TAG_LENGTH, output, olen);

    //@ crypto_chars_join(cont);
    //@ list<char> cs = append(tag_cs, sig_cs);
    //@ assert crypto_chars(secret, cont, size, cs);

    //@ close exists(sig_cg);
    //@ item e = asymmetric_signature_item(principal, count, some(pay), ent);
    /*@ if (col)
        {
          public_chars(cont, size);
          public_generated_split(polarssl_pub(pub), cs, TAG_LENGTH);
        }
    @*/
    //@ close item(payload, pay, pub);
    zeroize(output, (int) olen);
    //@ chars_join(output);
    free(output);
    //@ WELL_FORMED(tag_cs, sig_cs, TAG_ASYMMETRIC_SIG)
    //@ close ic_parts(e)(tag_cs, sig_cs);
    //@ close well_formed_item_chars(e)(pay_cs);
    //@ leak well_formed_item_chars(e)(pay_cs);
    //@ close item_constraints(e, cs, pub);
    //@ leak item_constraints(e, cs, pub);
    //@ close item(result, e, pub);
  }
  debug_print("SINGING RESULT:\n");
  print_item(result);

  return result;
}

void asymmetric_signature_verify(struct item *key, struct item *item,
                                 struct item *signature)
  /*@ requires [?f]world(?pub) &*&
               item(item, ?i, pub) &*& item(key, ?k, pub) &*&
               item(signature, ?sig, pub) &*&
                 sig == asymmetric_signature_item(?principal1, ?count1,
                                                  ?pay1, ?ent) &*&
               k == public_key_item(?principal2, ?count2); @*/
  /*@ ensures  [f]world(pub) &*&
               item(item, i, pub) &*& item(key, k, pub) &*&
               item(signature, sig, pub) &*&
               switch(pay1)
               {
                 case some(pay2):
                   return col || (principal1 == principal2 &&
                                  count1 == count2 && i == pay2);
                 case none:
                   return true == col;
               }; @*/
{
  debug_print("ASYM SIGNATURE CHECKING:\n");
  print_item(signature);
  check_is_asymmetric_signature(signature);

  {
    pk_context context;
    unsigned int olen;
    char* output;

    // Key
    //@ close pk_context(&context);
    //@ open [f]world(pub);
    pk_init(&context);
    //@ close [f]world(pub);
    set_public_key(&context, key);
    /*@ assert pk_context_with_key(&context, pk_public,
                                   ?principal3, ?count3, RSA_BIT_KEY_SIZE); @*/
    // Signature checking
    //@ open item(item, ?i_old, pub);
    //@ assert item->content |-> ?i_cont &*& item->size |-> ?i_size;
    //@ assert crypto_chars(secret, i_cont, i_size, ?i_cs);
    //@ assert [_]item_constraints(i, i_cs, pub);
    //@ open item(signature, _, pub);
    //@ assert signature->content |-> ?s_cont &*& signature->size |-> ?s_size;
    //@ assert crypto_chars(secret, s_cont, s_size, ?sig_cs);
    //@ open [_]item_constraints(sig, sig_cs, pub);
    //@ open ic_parts(sig)(?sig_tag, ?sig_cont);
    //@ take_append(TAG_LENGTH, sig_tag, sig_cont);
    //@ drop_append(TAG_LENGTH, sig_tag, sig_cont);
    //@ open [_]exists<cryptogram>(?cg_sig);
    //@ assert cg_sig == cg_asym_signature(principal1, count1, ?cs_pay, ent);

    if (item->size > RSA_KEY_SIZE)
      abort_crypto_lib("Assymetric signature checking failed: incorrect sizes");

    //@ crypto_chars_limits(s_cont);
    //@ crypto_chars_split(s_cont, TAG_LENGTH);
    //@ assert crypto_chars(secret, s_cont, TAG_LENGTH, sig_tag);
    //@ assert crypto_chars(secret, s_cont + TAG_LENGTH, ?s, sig_cont);
    //@ assert s == s_size - TAG_LENGTH;
    //@ if (col) cg_sig = chars_for_cg_sur(sig_cont, tag_asym_signature);
    //@ if (col) crypto_chars_to_chars(s_cont + TAG_LENGTH, s);
    //@ if (col) public_chars_extract(s_cont + TAG_LENGTH, cg_sig);
    //@ if (col) chars_to_secret_crypto_chars(s_cont + TAG_LENGTH, s);
    //@ close cryptogram(s_cont + TAG_LENGTH, s, sig_cont, cg_sig);
    if(pk_verify(&context, POLARSSL_MD_NONE, item->content,
                 (unsigned int) item->size,
                 signature->content + TAG_LENGTH,
                 (unsigned int) (signature->size - TAG_LENGTH)) != 0)
      abort_crypto_lib("Signing check failed: signature"
                       "was not created with the provided key");
    //@ pk_release_context_with_key(&context);
    pk_free(&context);
    //@ open pk_context(&context);
    //@ open cryptogram(s_cont + TAG_LENGTH, s, sig_cont, cg_sig);
    //@ crypto_chars_join(s_cont);
    //@ close item(signature, sig, pub);

    /*@ if (!col)
        {
          assert principal2 == principal3;
          assert count2 == count3;
          assert cs_pay == i_cs;
          open [_]item_constraints(i, cs_pay, pub);
          switch(pay1)
          {
            case some(pay2):
              assert [_]item_constraints(pay2, cs_pay, pub);
              item_constraints_injective(i, pay2, cs_pay);
            case none:
              open [_]ill_formed_item_chars(sig)(cs_pay);
              assert false;
          }
        }
    @*/
    //@ close item(item, i, pub);
  }
}