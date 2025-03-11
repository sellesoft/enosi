/**
 * @file tree-sitter grammar for lpp
 * @author sushi
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

function opToken($, tok)
{
  return alias(tok, $.operator)
}

function bracToken($, tok)
{
  return alias(tok, $.punc_bracket)
}

module.exports = grammar(
{
  name: "lpp",

  rules: 
  {
    source_file: $ => repeat(choice($.macro, $.document, $._lua)),
    
    document: $ => /[^@$]+/,

    _lua: $ => choice
    (
      seq(opToken($, '$'), $.lua_line),
      seq(opToken($, '$$$'), $.lua_block, opToken($, '$$$')),
      seq
      (
        opToken($, '$'), 
        bracToken($, token(prec(1,'('))), 
        $.lua_inline, 
        bracToken($, ')')),
    ),

    lua_line: $ => /.*\n/,

    lua_block: $ => repeat1(/./),

    lua_inline: $ => repeat1(/./),

    macro: $ => seq
    (
      choice($.macro_phase3, $.macro_phase2),
      $.macro_identifier,
      optional(choice
      (
        $.macro_tuple_args,
        $.macro_string_arg,
      )),
    ),

    macro_tuple_args: $ => seq
    (
      bracToken($, token(prec(1,'('))),
      optional(seq
      (
        $.macro_tuple_arg, repeat(seq(opToken($, ','), $.macro_tuple_arg)),
      )),
      bracToken($, ')'),
    ),

    macro_tuple_arg: $ => $._macro_tuple_arg_nest,

    _macro_tuple_arg_nest: $ => repeat1(choice
    (
      seq('(', optional($._macro_tuple_arg_nest), ')'),
      seq('{', optional($._macro_tuple_arg_nest), '}'),
      /./,
    )),

    macro_string_arg: $ => seq
    (
      token(prec(1, '"')),
      repeat(/./),
      '"',
    ),

    macro_identifier: $ => seq
    (
      $.macro_identifier_part,
      repeat(
        seq(opToken($, token(prec(1,'.'))), $.macro_identifier_part)),
      optional(seq(opToken($, token(prec(1, ':'))), $.macro_identifier_part))
    ),

    macro_identifier_part: $ => $._identifier,

    macro_phase2: $ => '@@',
    macro_phase3: $ => '@',

    _identifier: $ => /[a-zA-Z_]([a-zA-Z0-9_]+)?/
  }
});
