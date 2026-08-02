impl Parser<'_>
{
    fn expression(&mut self) -> Option<Expression>
    {
        let line = self.current()?;
        let rest = line.as_str();
        if let Some(value) = rest.strip_prefix("literal ")
        {
            self.index += 1;
            return Some(Expression::Literal {
                literal: self.literal(value),
                span: span(),
            });
        }
        if let Some(name) = rest.strip_prefix("local ")
        {
            self.index += 1;
            return Some(Expression::Local {
                name: name.to_string(),
                span: span(),
            });
        }
        if let Some(record) = rest.strip_prefix("field ")
        {
            return self.field_expression(record);
        }
        if let Some(record) = rest.strip_prefix("member ")
        {
            return self.member_expression(record);
        }
        if let Some(nominal_type) = rest.strip_prefix("object ")
        {
            return self.object_expression(nominal_type);
        }
        if let Some(record) = rest.strip_prefix("enum_data ")
        {
            return self.enum_data_expression(record);
        }
        if rest == "array"
        {
            return self.array_expression();
        }
        if rest == "set"
        {
            return self.set_expression();
        }
        if rest == "map"
        {
            return self.map_expression();
        }
        if let Some(tuple_type) = rest.strip_prefix("tuple ")
        {
            return self.tuple_expression(tuple_type);
        }
        if let Some(element) = rest.strip_prefix("tuple_element ")
        {
            return self.tuple_element_expression(element);
        }
        if let Some(element_type) = rest.strip_prefix("index ")
        {
            return self.index_expression(element_type);
        }
        if rest == "array_length"
        {
            return self.array_length_expression();
        }
        if let Some(target) = rest.strip_prefix("assign ")
        {
            self.index += 1;
            let value = self.expression().unwrap_or(Expression::Literal {
                literal: Literal::None,
                span: span(),
            });
            return Some(Expression::Assign {
                target: target.to_string(),
                value: Box::new(value),
                span: span(),
            });
        }
        if let Some(record) = rest.strip_prefix("assign_field ")
        {
            return self.assign_field_expression(record);
        }
        if let Some(update) = rest.strip_prefix("update ")
        {
            self.index += 1;
            let fields = update.split_whitespace().collect::<Vec<_>>();
            if fields.len() != 3
            {
                self.report("invalid update expression".to_string());
                return None;
            }
            let position = match fields[0]
            {
                "prefix" => UpdatePosition::Prefix,
                "postfix" => UpdatePosition::Postfix,
                value =>
                {
                    self.report(format!("unknown update position '{value}'"));
                    return None;
                }
            };
            let operator = match fields[1]
            {
                "increment" => UpdateOperator::Increment,
                "decrement" => UpdateOperator::Decrement,
                value =>
                {
                    self.report(format!("unknown update operator '{value}'"));
                    return None;
                }
            };
            return Some(Expression::Update {
                target: fields[2].to_string(),
                operator,
                position,
                span: span(),
            });
        }
        if let Some(signature) = rest.strip_prefix("call ")
        {
            self.index += 1;
            let Some((function, signature)) = signature.split_once(" : (")
            else
            {
                self.report("invalid call signature".to_string());
                return None;
            };
            let Some((parameters, return_type)) = signature.split_once(") -> ")
            else
            {
                self.report("invalid call return signature".to_string());
                return None;
            };
            let parameter_types = if parameters.is_empty()
            {
                Vec::new()
            }
            else
            {
                split_type_list(parameters)
                    .into_iter()
                    .map(|name| {
                        self.parse_type(name.trim())
                            .unwrap_or(Type::Named(name.trim().to_string()))
                    })
                    .collect()
            };
            let return_type = self
                .parse_type(return_type)
                .unwrap_or(Type::Named(return_type.to_string()));
            let mut arguments = Vec::with_capacity(parameter_types.len());
            for _ in 0..parameter_types.len()
            {
                self.consume_expression_field("argument");
                arguments.push(self.expression().unwrap_or(Expression::Literal {
                    literal: Literal::None,
                    span: span(),
                }));
            }
            return Some(Expression::Call {
                function: function.to_string(),
                arguments,
                parameter_types,
                return_type: Box::new(return_type),
                span: span(),
            });
        }
        if let Some(result_type) = rest.strip_prefix("if_expression ")
        {
            self.index += 1;
            let result_type = self
                .parse_type(result_type)
                .unwrap_or(Type::Named(result_type.to_string()));
            self.consume_expression_field("condition");
            let condition = self.expression().unwrap_or(Expression::Literal {
                literal: Literal::None,
                span: span(),
            });
            let then_block = self.named_block("then");
            let else_block = self.named_block("else");
            return Some(Expression::If {
                condition: Box::new(condition),
                then_block: Box::new(then_block),
                else_block: Box::new(else_block),
                result_type: Box::new(result_type),
                span: span(),
            });
        }
        if let Some(signature) = rest.strip_prefix("match_expression ")
        {
            return self.match_expression(signature);
        }
        if let Some(operator) = rest.strip_prefix("binary ")
        {
            self.index += 1;
            let operator = self.binary_operator(operator).unwrap_or(BinaryOperator::Add);
            self.consume_expression_field("left");
            let left = self.expression().unwrap_or(Expression::Literal {
                literal: Literal::None,
                span: span(),
            });
            self.consume_expression_field("right");
            let right = self.expression().unwrap_or(Expression::Literal {
                literal: Literal::None,
                span: span(),
            });
            return Some(Expression::Binary {
                operator,
                left: Box::new(left),
                right: Box::new(right),
                span: span(),
            });
        }
        if let Some(operator) = rest.strip_prefix("unary ")
        {
            self.index += 1;
            let operator = self.unary_operator(operator).unwrap_or(UnaryOperator::Positive);
            self.consume_expression_field("operand");
            let operand = self.expression().unwrap_or(Expression::Literal {
                literal: Literal::None,
                span: span(),
            });
            return Some(Expression::Unary {
                operator,
                operand: Box::new(operand),
                span: span(),
            });
        }
        if let Some(element_type) = rest.strip_prefix("optional_unwrap ")
        {
            self.index += 1;
            let element_type = self
                .parse_type(element_type)
                .unwrap_or(Type::Named(element_type.to_string()));
            self.consume_expression_field("value");
            let value = self.expression().unwrap_or(Expression::Literal {
                literal: Literal::None,
                span: span(),
            });
            return Some(Expression::OptionalUnwrap {
                value: Box::new(value),
                element_type: Box::new(element_type),
                span: span(),
            });
        }
        if let Some(record) = rest.strip_prefix("optional_coalesce_assign ")
        {
            self.index += 1;
            let (target, optional_type) = record.split_once(" : ").unwrap_or((record, "Optional<()>"));
            let optional_type = self
                .parse_type(optional_type)
                .unwrap_or(Type::Named(optional_type.to_string()));
            self.consume_expression_field("value");
            let value = self.expression().unwrap_or(Expression::Literal {
                literal: Literal::None,
                span: span(),
            });
            return Some(Expression::OptionalCoalesceAssign {
                target: target.to_string(),
                value: Box::new(value),
                optional_type: Box::new(optional_type),
                span: span(),
            });
        }
        if let Some(record) = rest.strip_prefix("optional_member ")
        {
            self.index += 1;
            let (member, types) = record.split_once(" : ").unwrap_or((record, "() -> Optional<()>"));
            let (owner, name) = member.rsplit_once("::").unwrap_or((member, ""));
            let (field_type, result_type) = types.split_once(" -> ").unwrap_or((types, "Optional<()>"));
            let field_type = self
                .parse_type(field_type)
                .unwrap_or(Type::Named(field_type.to_string()));
            let result_type = self
                .parse_type(result_type)
                .unwrap_or(Type::Named(result_type.to_string()));
            self.consume_expression_field("receiver");
            let receiver = self.expression().unwrap_or(Expression::Literal {
                literal: Literal::None,
                span: span(),
            });
            return Some(Expression::OptionalMember {
                receiver: Box::new(receiver),
                owner: owner.to_string(),
                name: name.to_string(),
                field_type: Box::new(field_type),
                result_type: Box::new(result_type),
                span: span(),
            });
        }
        if rest == "propagate"
        {
            self.index += 1;
            let value = self.expression().unwrap_or(Expression::Literal {
                literal: Literal::None,
                span: span(),
            });
            return Some(Expression::ResultPropagation {
                value: Box::new(value),
                span: span(),
            });
        }
        None
    }

    fn consume_expression_field(&mut self, field: &str)
    {
        let Some(line) = self.current()
        else
        {
            self.report(format!("missing binary {field} expression"));
            return;
        };
        if line == field
        {
            self.index += 1;
        }
        else
        {
            self.report(format!("expected binary {field} expression"));
        }
    }

    fn binary_operator(&mut self, name: &str) -> Option<BinaryOperator>
    {
        match name
        {
            "add" => Some(BinaryOperator::Add),
            "sub" => Some(BinaryOperator::Sub),
            "mul" => Some(BinaryOperator::Mul),
            "div" => Some(BinaryOperator::Div),
            "rem" => Some(BinaryOperator::Rem),
            "bit_and" => Some(BinaryOperator::BitAnd),
            "bit_or" => Some(BinaryOperator::BitOr),
            "bit_xor" => Some(BinaryOperator::BitXor),
            "logical_and" => Some(BinaryOperator::LogicalAnd),
            "logical_or" => Some(BinaryOperator::LogicalOr),
            "coalesce" => Some(BinaryOperator::Coalesce),
            "shift_left" => Some(BinaryOperator::ShiftLeft),
            "shift_right" => Some(BinaryOperator::ShiftRight),
            "eq" => Some(BinaryOperator::Equal),
            "ne" => Some(BinaryOperator::NotEqual),
            "lt" => Some(BinaryOperator::Less),
            "le" => Some(BinaryOperator::LessEqual),
            "gt" => Some(BinaryOperator::Greater),
            "ge" => Some(BinaryOperator::GreaterEqual),
            _ =>
            {
                self.report(format!("unknown binary operator '{name}'"));
                None
            }
        }
    }

    fn local_type(&mut self) -> Type
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing local type".to_string());
            return Type::Named(String::new());
        };
        self.index += 1;
        let Some(name) = line.strip_prefix("type ")
        else
        {
            self.report("expected local type".to_string());
            return Type::Named(String::new());
        };
        self.parse_type(name).unwrap_or(Type::Named(name.to_string()))
    }

    fn local_mutability(&mut self) -> bool
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing local mutability".to_string());
            return false;
        };
        self.index += 1;
        match line.strip_prefix("mutability ")
        {
            Some("mutable") => true,
            Some("immutable") => false,
            Some(value) =>
            {
                self.report(format!("unknown mutability '{value}'"));
                false
            }
            None =>
            {
                self.report("expected local mutability".to_string());
                false
            }
        }
    }

    fn parse_type(&mut self, name: &str) -> Option<Type>
    {
        parse_type_text(name).or_else(|| {
            self.report(format!("unknown type '{name}'"));
            None
        })
    }

    fn import(&mut self, module: &mut Module)
    {
        self.index += 1;
        while let Some(line) = self.current()
        {
            if line.is_empty()
            {
                self.index += 1;
                continue;
            }
            if line == ".end"
            {
                self.index += 1;
                break;
            }
            if !line.starts_with("import ")
            {
                break;
            }
            if let Some(import) = parse_import_line(&line)
            {
                module.imports.push(import);
            }
            else
            {
                self.report(format!("invalid import record '{line}'"));
            }
            self.index += 1;
        }
    }

    fn declarations(&mut self, module: &mut Module)
    {
        self.index += 1;
        while let Some(line) = self.current()
        {
            if line.is_empty()
            {
                self.index += 1;
                continue;
            }
            if line == ".end"
            {
                self.index += 1;
                break;
            }
            let Some(name) = line.strip_prefix("symbol ")
            else
            {
                break;
            };
            self.index += 1;
            let kind = self.symbol_kind();
            let visibility = self.visibility();
            module.symbols.push(Symbol {
                name: name.to_string(),
                kind,
                visibility,
                span: span(),
            });
        }
    }

    fn symbol_kind(&mut self) -> SymbolKind
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing symbol kind".to_string());
            return SymbolKind::Function;
        };
        self.index += 1;
        let Some(name) = line.strip_prefix("kind ")
        else
        {
            self.report("expected symbol kind".to_string());
            return SymbolKind::Function;
        };
        match name
        {
            "function" => SymbolKind::Function,
            "class" => SymbolKind::Class,
            "interface" => SymbolKind::Interface,
            "enum" => SymbolKind::Enum,
            "data" => SymbolKind::Data,
            "macro" => SymbolKind::Macro,
            _ =>
            {
                self.report(format!("unknown symbol kind '{name}'"));
                SymbolKind::Function
            }
        }
    }

    fn visibility(&mut self) -> Visibility
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing symbol visibility".to_string());
            return Visibility::Private;
        };
        self.index += 1;
        let Some(name) = line.strip_prefix("visibility ")
        else
        {
            self.report("expected symbol visibility".to_string());
            return Visibility::Private;
        };
        match name
        {
            "public" => Visibility::Public,
            "internal" => Visibility::Internal,
            "private" => Visibility::Private,
            _ =>
            {
                self.report(format!("unknown visibility '{name}'"));
                Visibility::Private
            }
        }
    }

    fn xhir_version(&mut self)
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing XHIR version header".to_string());
            return;
        };
        let Some(version) = line.strip_prefix(".xhir version ")
        else
        {
            self.report("expected XHIR version header".to_string());
            self.index += 1;
            return;
        };
        match version.parse::<u32>()
        {
            Ok(version) if is_supported_xhir_version(version) =>
            {}
            Ok(version) => self.report(format!(
                "unsupported XHIR version {version}; supported versions are 0 and {SUPPORTED_XHIR_VERSION}"
            )),
            Err(_) => self.report("invalid XHIR version number".to_string()),
        }
        self.index += 1;
    }

    fn next_non_empty(&mut self) -> Option<String>
    {
        while self.current().as_deref() == Some("")
        {
            self.index += 1;
        }
        self.current()
    }

    fn current(&self) -> Option<String>
    {
        self.lines.get(self.index).map(|line| line.trim_start().to_string())
    }

    fn report(&mut self, message: String)
    {
        self.diagnostics.push(XhirParseDiagnostic {
            line: self.index + 1,
            message,
        });
    }
}
