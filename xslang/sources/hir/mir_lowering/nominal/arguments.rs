impl HirToMirLowerer
{
    fn append_nominal_aggregate_arguments(
        &mut self,
        definition: &crate::hir::declarations::NominalType,
        aggregate: mir::LocalId,
        span: Span,
        lowered: &mut mir::Function,
        arguments: &mut Vec<mir::LocalId>,
        visiting: &mut Vec<String>,
    ) -> Option<()>
    {
        for (index, field) in self.resolved_nominal_fields(definition)?.iter().enumerate()
        {
            let field_type = crate::hir::declarations::type_ref_to_checked(&field.ty)?;
            let value_type = self.lower_value_type(&field_type, span)?;
            let value = self.declare_temp(value_type, span, lowered)?;
            self.current_block_mut(lowered)
                .statements
                .push(mir::Statement::Extract {
                    result: value,
                    aggregate,
                    field: u32::try_from(index).ok()?,
                    field_type: value_type,
                    span,
                });
            match field_type
            {
                Type::Primitive(_) => arguments.push(value),
                Type::Named(ref nested_type) =>
                {
                    if visiting.contains(nested_type)
                    {
                        self.report(
                            DiagnosticCode::UnsupportedType,
                            "recursive data aggregate argument requires an indirect ABI",
                            span,
                        );
                        return None;
                    }
                    let nested = self.nominal_types.get(nested_type)?.clone();
                    visiting.push(nested_type.clone());
                    self.append_nominal_aggregate_arguments(&nested, value, span, lowered, arguments, visiting)?;
                    visiting.pop();
                }
                _ =>
                {
                    self.report(
                        DiagnosticCode::UnsupportedType,
                        "data aggregate field has no scalar native call ABI",
                        span,
                    );
                    return None;
                }
            }
        }
        Some(())
    }

    fn append_nominal_parameters(
        &mut self,
        root: &str,
        prefix: &[String],
        definition: &crate::hir::declarations::NominalType,
        span: Span,
        lowered: &mut mir::Function,
        visiting: &mut Vec<String>,
    )
    {
        let Some(definition_fields) = self.resolved_nominal_fields(definition)
        else
        {
            return;
        };
        for field in &definition_fields
        {
            let Some(field_type) = crate::hir::declarations::type_ref_to_checked(&field.ty)
            else
            {
                continue;
            };
            let mut path = prefix.to_vec();
            path.push(field.name.clone());
            match field_type
            {
                Type::Primitive(_) =>
                {
                    let Some(value_type) = self.lower_value_type(&field_type, span)
                    else
                    {
                        continue;
                    };
                    let id = mir::LocalId(self.next_local);
                    self.next_local += 1;
                    let name = field_key(root, &path);
                    self.field_locals.insert(name.clone(), id);
                    lowered.parameters.push(mir::Parameter {
                        local: id,
                        name,
                        value_type,
                        span,
                    });
                }
                Type::Named(ref nested_type) =>
                {
                    if visiting.contains(nested_type)
                    {
                        self.report(
                            DiagnosticCode::UnsupportedType,
                            format!(
                                "recursive data parameter field '{}' requires an indirect ABI",
                                field_key(root, &path)
                            ),
                            span,
                        );
                        continue;
                    }
                    let Some(nested) = self.nominal_types.get(nested_type).cloned()
                    else
                    {
                        self.report(
                            DiagnosticCode::UnsupportedType,
                            format!(
                                "nested parameter field '{}' has no HIR declaration",
                                field_key(root, &path)
                            ),
                            span,
                        );
                        continue;
                    };
                    visiting.push(nested_type.clone());
                    self.append_nominal_parameters(root, &path, &nested, span, lowered, visiting);
                    visiting.pop();
                }
                _ => self.report(
                    DiagnosticCode::UnsupportedType,
                    format!("parameter field '{}' has no scalar native ABI", field_key(root, &path)),
                    span,
                ),
            }
        }
    }

    fn append_nominal_place_arguments(
        &mut self,
        root: &str,
        prefix: &[String],
        definition: &crate::hir::declarations::NominalType,
        span: Span,
        lowered: &mut mir::Function,
        arguments: &mut NominalArguments<'_>,
    ) -> Option<()>
    {
        for field in &self.resolved_nominal_fields(definition)?
        {
            let field_type = crate::hir::declarations::type_ref_to_checked(&field.ty)?;
            let mut path = prefix.to_vec();
            path.push(field.name.clone());
            match field_type
            {
                Type::Primitive(_) =>
                {
                    let value_type = self.lower_value_type(&field_type, span)?;
                    arguments.values.push(self.lower_field_load(
                        &FieldPath {
                            root: root.to_string(),
                            fields: path,
                            ty: field_type,
                            mutable: false,
                            span,
                        },
                        value_type,
                        lowered,
                    )?);
                }
                Type::Named(ref nested_type) =>
                {
                    if arguments.visiting.contains(nested_type)
                    {
                        self.report(
                            DiagnosticCode::UnsupportedType,
                            format!(
                                "recursive data argument field '{}' requires an indirect ABI",
                                field_key(root, &path)
                            ),
                            span,
                        );
                        return None;
                    }
                    let nested = self.nominal_types.get(nested_type)?.clone();
                    arguments.visiting.push(nested_type.clone());
                    self.append_nominal_place_arguments(root, &path, &nested, span, lowered, arguments)?;
                    arguments.visiting.pop();
                }
                _ => return None,
            }
        }
        Some(())
    }

    fn append_nominal_object_arguments(
        &mut self,
        definition: &crate::hir::declarations::NominalType,
        initializers: &[crate::hir::type_check::ObjectField],
        span: Span,
        lowered: &mut mir::Function,
        arguments: &mut Vec<mir::LocalId>,
        visiting: &mut Vec<String>,
    ) -> Option<()>
    {
        for field in &self.resolved_nominal_fields(definition)?
        {
            let initializer = initializers.iter().find(|candidate| candidate.name == field.name)?;
            let field_type = crate::hir::declarations::type_ref_to_checked(&field.ty)?;
            match field_type
            {
                Type::Primitive(_) =>
                {
                    let value_type = self.lower_value_type(&field_type, span)?;
                    arguments.push(self.lower_expression_to_local(&initializer.value, value_type, lowered)?);
                }
                Type::Named(ref nested_type) =>
                {
                    if visiting.contains(nested_type)
                    {
                        self.report(
                            DiagnosticCode::UnsupportedType,
                            "recursive data object arguments require an indirect ABI",
                            initializer.span,
                        );
                        return None;
                    }
                    let nested = self.nominal_types.get(nested_type)?.clone();
                    let Expression::Object {
                        nominal_type,
                        fields,
                        ..
                    } = &initializer.value
                    else
                    {
                        return None;
                    };
                    if nominal_type != nested_type
                    {
                        return None;
                    }
                    visiting.push(nested_type.clone());
                    self.append_nominal_object_arguments(
                        &nested,
                        fields,
                        initializer.span,
                        lowered,
                        arguments,
                        visiting,
                    )?;
                    visiting.pop();
                }
                _ => return None,
            }
        }
        Some(())
    }

    fn nominal_place_locals(
        &self,
        root: &str,
        prefix: &[String],
        definition: &crate::hir::declarations::NominalType,
    ) -> Option<Vec<mir::LocalId>>
    {
        let mut locals = Vec::new();
        self.append_nominal_place_locals(
            root,
            prefix,
            definition,
            &mut vec![definition.name.clone()],
            &mut locals,
        )?;
        Some(locals)
    }

    fn append_nominal_place_locals(
        &self,
        root: &str,
        prefix: &[String],
        definition: &crate::hir::declarations::NominalType,
        visiting: &mut Vec<String>,
        locals: &mut Vec<mir::LocalId>,
    ) -> Option<()>
    {
        for field in &self.resolved_nominal_fields(definition)?
        {
            let field_type = crate::hir::declarations::type_ref_to_checked(&field.ty)?;
            let mut path = prefix.to_vec();
            path.push(field.name.clone());
            match field_type
            {
                Type::Primitive(_) => locals.push(*self.field_locals.get(&field_key(root, &path))?),
                Type::Named(ref nested_type) =>
                {
                    if visiting.contains(nested_type)
                    {
                        return None;
                    }
                    let nested = self.nominal_types.get(nested_type)?;
                    visiting.push(nested_type.clone());
                    self.append_nominal_place_locals(root, &path, nested, visiting, locals)?;
                    visiting.pop();
                }
                _ => return None,
            }
        }
        Some(())
    }

    fn lower_object_fields(
        &mut self,
        root: &str,
        prefix: &[String],
        definition: &crate::hir::declarations::NominalType,
        initializers: &[crate::hir::type_check::ObjectField],
        fallback_span: Span,
        lowered: &mut mir::Function,
    )
    {
        let Some(definition_fields) = self.resolved_nominal_fields(definition)
        else
        {
            return;
        };
        for field in &definition_fields
        {
            let Some(initializer) = initializers.iter().find(|candidate| candidate.name == field.name)
            else
            {
                continue;
            };
            let Some(value_type) = crate::hir::declarations::type_ref_to_checked(&field.ty)
            else
            {
                continue;
            };
            let mut path = prefix.to_vec();
            path.push(field.name.clone());
            match value_type
            {
                Type::Primitive(_) =>
                {
                    let key = field_key(root, &path);
                    let field_local =
                        self.declare_local(key.clone(), &value_type, field.mutable, initializer.span, lowered);
                    self.field_locals.insert(key, field_local);
                    self.lower_assignment(field_local, &initializer.value, lowered);
                }
                Type::Named(ref nested_type) =>
                {
                    let Some(nested_definition) = self.nominal_types.get(nested_type).cloned()
                    else
                    {
                        self.report(
                            DiagnosticCode::UnsupportedType,
                            format!(
                                "nested nominal field '{}' has no HIR declaration",
                                field_key(root, &path)
                            ),
                            initializer.span,
                        );
                        continue;
                    };
                    let Expression::Object {
                        nominal_type,
                        fields,
                        ..
                    } = &initializer.value
                    else
                    {
                        self.report(
                            DiagnosticCode::UnsupportedExpression,
                            format!(
                                "nested nominal field '{}' requires an object initializer",
                                field_key(root, &path)
                            ),
                            initializer.span,
                        );
                        continue;
                    };
                    if nominal_type != nested_type
                    {
                        self.report(
                            DiagnosticCode::UnsupportedType,
                            format!(
                                "nested object initializer for '{}' has the wrong nominal type",
                                field_key(root, &path)
                            ),
                            initializer.span,
                        );
                        continue;
                    }
                    self.lower_object_fields(root, &path, &nested_definition, fields, fallback_span, lowered);
                }
                _ => self.report(
                    DiagnosticCode::UnsupportedType,
                    format!("field '{}' is not scalar-lowerable yet", field_key(root, &path)),
                    fallback_span,
                ),
            }
        }
    }

    pub(super) fn resolved_nominal_fields(
        &self,
        definition: &crate::hir::declarations::NominalType,
    ) -> Option<Vec<crate::hir::declarations::Field>>
    {
        crate::hir::declarations::resolved_fields(definition, &self.nominal_types).ok()
    }
}

fn field_key(root: &str, fields: &[String]) -> String
{
    std::iter::once(root)
        .chain(fields.iter().map(String::as_str))
        .collect::<Vec<_>>()
        .join(".")
}
