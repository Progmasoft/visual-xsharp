/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
  pub fn lower_function_with_parameters(mut self,
                                        function: &Function,
                                        parameter_count: usize)
                                        -> Result<mir::Function, Vec<Diagnostic>>
  {
    let return_type = self.lower_return_type(function.return_type.as_ref(), Span::new(0, 0, 0));
    let mut lowered = mir::Function { name: function.name.clone(),
                                      parameters: vec![],
                                      return_type,
                                      locals: vec![],
                                      blocks: vec![mir::BasicBlock { id: mir::BlockId(0),
                                                                     statements: vec![],
                                                                     terminator: None,
                                                                     span: Span::new(0, 0, 0) }] };
    for (index, local) in function.locals.iter().enumerate()
    {
      if index < parameter_count
      {
        if matches!(local.ty, Type::Named(_))
        {
          self.lower_nominal_parameter(local, &mut lowered);
          continue;
        }
        let id = mir::LocalId(self.next_local);
        self.next_local += 1;
        if let Some(value_type) = self.lower_value_type(&local.ty, local.span)
        {
          self.locals.insert(local.name.clone(), id);
          lowered.parameters.push(mir::Parameter { local: id,
                                                   name: local.name.clone(),
                                                   value_type,
                                                   span: local.span });
        }
      }
      else
      {
        self.declare_local(local.name.clone(), &local.ty, local.mutable, local.span, &mut lowered);
      }
    }
    for statement in &function.body
    {
      self.lower_statement(statement, &mut lowered);
    }
    if !self.current_is_terminated(&mut lowered)
    {
      if lowered.return_type == XlilType::VOID
      {
        self.set_terminator(mir::Terminator::Return(None), Span::new(0, 0, 0), &mut lowered);
      }
      else
      {
        self.report(DiagnosticCode::MissingReturn,
                    "non-void HIR function did not lower to a MIR return",
                    Span::new(0, 0, 0));
      }
    }
    if self.diagnostics.is_empty()
    {
      Ok(lowered)
    }
    else
    {
      Err(self.diagnostics)
    }
  }
}
